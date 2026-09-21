Binary model files (`*.rbxm`) the harness (R42) reads, from rojo-rbx/rbx-test-files
(`models/<name>/binary.rbxm`, MIT): real files as Studio wrote them, so the reader is
checked against the format rather than against itself. The `.rbxmx` beside some is the
same model as Studio's XML (`models/<name>/xml.rbxmx`), for reading the two against each
other; `baseplate-566.rbxl` / `.rbxlx` is `places/baseplate-566`, a whole place Studio
saved, for the Studio's Open Roblox Place. `two-terrainregions` has empty voxel grids
(`AQU=`), as does the baseplate's Terrain: no file here holds real terrain voxels yet.
`imagelabel-content` and `content-mixed` (Studio 0.663) carry Content properties: value type
0x22 in the binary, `<uri>` / `<null>` in the XML, beside a Decal's older ContentId Texture.
