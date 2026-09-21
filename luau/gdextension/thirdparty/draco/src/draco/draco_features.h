// Draco's build normally writes this file from its CMake options. There is no CMake step here:
// the decoder is compiled straight into the extension, as meshoptimizer is, so the feature set
// is stated once, here.
//
// Every feature a Roblox mesh needs to decode, and nothing else. Point-cloud compression, the
// transcoder and the glTF/scene layer are left out because nothing asks for them.
#ifndef DRACO_FEATURES_H_
#define DRACO_FEATURES_H_

#define DRACO_MESH_COMPRESSION_SUPPORTED
#define DRACO_NORMAL_ENCODING_SUPPORTED
#define DRACO_STANDARD_EDGEBREAKER_SUPPORTED
#define DRACO_PREDICTIVE_EDGEBREAKER_SUPPORTED
#define DRACO_BACKWARDS_COMPATIBILITY_SUPPORTED
#define DRACO_ATTRIBUTE_INDICES_DEDUPLICATION_SUPPORTED
#define DRACO_ATTRIBUTE_VALUES_DEDUPLICATION_SUPPORTED

#endif  // DRACO_FEATURES_H_
