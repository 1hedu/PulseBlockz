#include "script_worker.h"

#include <chrono>
#include <utility>

namespace pulseblockz {

static double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

ScriptWorker::ScriptWorker(Options opts, SetupFn setup)
    : opts_(opts), setup_(std::move(setup)), threaded_(opts.threaded) {
    if (threaded_) {
        thread_ = std::thread([this] { loop(); });
    } else {
        ensureSandbox();
    }
}

ScriptWorker::~ScriptWorker() {
    if (threaded_) {
        submit(Job{Job::Stop});
        if (thread_.joinable()) thread_.join();
    }
    // Clear the wake before the Sandbox goes: it must not fire once this destructor runs.
    { std::lock_guard<std::mutex> lk(evMu_); wake_ = nullptr; }
    sb_.reset();
}

void ScriptWorker::ensureSandbox() {
    if (sb_) return;
    sb_ = std::make_unique<Sandbox>(opts_.budget);
    sb_->setUserData(this);
    sb_->setPrintHandler([this](const std::string& s) {
        HostEvent e; e.type = HostEvent::Print; e.script = current_;
        e.name = currentScriptName(); e.text = s;
        emit(std::move(e));
    });
    if (setup_) setup_(*sb_);
}

const std::string& ScriptWorker::currentScriptName() const {
    static const std::string none;
    auto it = names_.find(current_);
    return it == names_.end() ? none : it->second;
}

// ---- host-thread API -------------------------------------------------------------
int ScriptWorker::load(const std::string& name, const std::string& source) {
    int h = nextHandle_++;
    Job j{Job::Load}; j.handle = h; j.a = name; j.b = source;
    submit(std::move(j));
    return h;
}
void ScriptWorker::unload(int handle) { Job j{Job::Unload}; j.handle = handle; submit(std::move(j)); }
void ScriptWorker::runMain(int handle) { Job j{Job::RunMain}; j.handle = handle; submit(std::move(j)); }
void ScriptWorker::call(int handle, const std::string& fn, double arg) {
    Job j{Job::Call}; j.handle = handle; j.a = fn; j.num = arg; submit(std::move(j));
}
void ScriptWorker::setBudget(const Budget& b, int maxCommandsPerCall) {
    Job j{Job::SetBudget}; j.budget = b; j.maxCommands = maxCommandsPerCall; submit(std::move(j));
}

void ScriptWorker::tick(double delta) {
    if (threaded_) {
        std::lock_guard<std::mutex> lk(jobMu_);
        // jobs_ holds queued jobs only, never the running one, so any Tick here is safe to fold into.
        for (auto it = jobs_.rbegin(); it != jobs_.rend(); ++it) {
            if (it->kind == Job::Tick) { it->num += delta; return; }
        }
        Job j{Job::Tick}; j.num = delta;
        jobs_.push_back(std::move(j));
        jobCv_.notify_one();
        return;
    }
    Job j{Job::Tick}; j.num = delta; submit(std::move(j));
}

void ScriptWorker::submit(Job j) {
    if (!threaded_) { execute(j); return; }
    { std::lock_guard<std::mutex> lk(jobMu_); jobs_.push_back(std::move(j)); }
    jobCv_.notify_one();
}

void ScriptWorker::waitIdle() {
    if (!threaded_) return;
    std::unique_lock<std::mutex> lk(jobMu_);
    idleCv_.wait(lk, [this] { return jobs_.empty() && !running_; });
}

bool ScriptWorker::idle() const {
    if (!threaded_) return true;
    std::lock_guard<std::mutex> lk(jobMu_);
    return jobs_.empty() && !running_;
}

std::vector<HostEvent> ScriptWorker::drain() {
    std::vector<HostEvent> out;
    std::lock_guard<std::mutex> lk(evMu_);
    out.swap(events_);
    return out;
}

void ScriptWorker::setWakeHandler(std::function<void()> fn) {
    std::lock_guard<std::mutex> lk(evMu_);
    wake_ = std::move(fn);
}

void ScriptWorker::emit(HostEvent ev) {
    std::function<void()> wake;
    {
        std::lock_guard<std::mutex> lk(evMu_);
        bool wasEmpty = events_.empty();
        events_.push_back(std::move(ev));
        if (wasEmpty && wake_) wake = wake_;   // copy: the handler may run unlocked
    }
    if (wake) wake();
}

bool ScriptWorker::command(HostEvent ev) {
    if (opts_.maxCommandsPerCall > 0 && commandsThisCall_ >= opts_.maxCommandsPerCall) return false;
    commandsThisCall_++;
    ev.type = HostEvent::Command;
    emit(std::move(ev));
    return true;
}

// ---- worker thread ---------------------------------------------------------------
void ScriptWorker::loop() {
    ensureSandbox();
    for (;;) {
        Job j;
        {
            std::unique_lock<std::mutex> lk(jobMu_);
            jobCv_.wait(lk, [this] { return !jobs_.empty(); });
            j = std::move(jobs_.front());
            jobs_.pop_front();
            running_ = true;
        }
        if (j.kind == Job::Stop) {
            std::lock_guard<std::mutex> lk(jobMu_);
            running_ = false;
            idleCv_.notify_all();
            return;
        }
        execute(j);
        {
            std::lock_guard<std::mutex> lk(jobMu_);
            running_ = false;
            if (jobs_.empty()) idleCv_.notify_all();
        }
    }
}

void ScriptWorker::execute(Job& j) {
    switch (j.kind) {
    case Job::Load: {
        std::string err;
        int sh = sb_->loadScript(j.a, j.b, &err);
        if (sh == 0) {
            HostEvent e; e.type = HostEvent::LoadError; e.script = j.handle; e.name = j.a; e.text = err;
            emit(std::move(e));
            break;
        }
        sbHandles_[j.handle] = sh;
        names_[j.handle] = j.a;
        order_.push_back(j.handle);
        break;
    }
    case Job::Unload: {
        auto it = sbHandles_.find(j.handle);
        if (it == sbHandles_.end()) break;
        sb_->unloadScript(it->second);
        sbHandles_.erase(it);
        names_.erase(j.handle);
        for (size_t i = 0; i < order_.size(); i++)
            if (order_[i] == j.handle) { order_.erase(order_.begin() + i); break; }
        break;
    }
    case Job::RunMain:
    case Job::Call: {
        auto it = sbHandles_.find(j.handle);
        if (it == sbHandles_.end()) break;   // failed load or already unloaded: no-op
        current_ = j.handle; commandsThisCall_ = 0;
        // An explicit host request is its own frame, not the remains of the last tick batch.
        sb_->beginFrame();
        RunResult r = (j.kind == Job::RunMain) ? sb_->runMain(it->second)
                                               : sb_->call(it->second, j.a.c_str(), j.num);
        current_ = 0;
        if (!r.ok && !r.skipped) {
            HostEvent k; k.type = HostEvent::Killed; k.script = j.handle; k.name = names_[j.handle];
            k.text = r.error; k.result = r;
            emit(std::move(k));
        }
        HostEvent e; e.type = HostEvent::Result; e.script = j.handle;
        e.name = (j.kind == Job::RunMain) ? "main" : j.a; e.result = r;
        emit(std::move(e));
        break;
    }
    case Job::Tick:
        doTick(j.num);
        break;
    case Job::SetBudget:
        opts_.budget = j.budget;
        if (j.maxCommands >= 0) opts_.maxCommandsPerCall = j.maxCommands;
        sb_->setBudget(j.budget);
        break;
    case Job::Stop:
        break;
    }
    memUsed_.store(sb_->memoryNow(), std::memory_order_relaxed);
}

void ScriptWorker::doTick(double delta) {
    double t0 = nowMs();
    sb_->beginFrame();
    int ran = 0, killed = 0, skipped = 0;
    size_t n = order_.size();
    // Rotating start, so a frame-budget squeeze does not starve the same scripts every tick.
    if (n) rr_ %= n;
    for (size_t i = 0; i < n; i++) {
        int h = order_[(rr_ + i) % n];
        current_ = h; commandsThisCall_ = 0;
        RunResult r = sb_->call(sbHandles_[h], "Tick", delta);
        current_ = 0;
        if (r.skipped) { skipped++; continue; }
        ran++;
        if (!r.ok) {
            killed++;
            HostEvent k; k.type = HostEvent::Killed; k.script = h; k.name = names_[h];
            k.text = r.error; k.result = r;
            emit(std::move(k));
        }
    }
    if (n) rr_ = (rr_ + 1) % n;
    HostEvent s; s.type = HostEvent::TickStats;
    s.result.ok = killed == 0 && skipped == 0;
    s.result.millisUsed = nowMs() - t0;
    s.result.memoryNow = sb_->memoryNow(); s.result.memoryPeak = sb_->memoryPeak();
    s.x = delta; s.id = ran; s.y = killed; s.z = skipped;
    emit(std::move(s));
}

} // namespace pulseblockz
