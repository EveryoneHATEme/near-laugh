# scene-texturing Specification

## Purpose

Defines packaged structural and model base-color materials, deterministic texture mapping, supported sampling and opaque or alpha-cutout coverage for the authored game scene.

## Requirements

### Requirement: Fixed prototype surface texture set
Every generated structural solid and present terrain SHALL select one known packaged opaque structural material. A solid's material SHALL be independent of its collision kind, and one choice SHALL cover all its faces. Terrain SHALL use one material for its entire surface. The finite catalog SHALL contain the three legacy prototype appearances and the selected wood-floor and wallpaper appearances, and assignments SHALL remain immutable for a run. Generated switches and doors SHALL retain their explicit legacy obstacle material. Removed shooting-target identities SHALL remain unsupported.

#### Scenario: Prototype level assigns surface roles
- **WHEN** the immutable prototype level is constructed
- **THEN** every solid and present terrain has one valid structural material without requiring its appearance to match its collision kind

#### Scenario: Movement-test geometry is assigned
- **WHEN** the walkable step or low-clearance structure is prepared for rendering
- **THEN** its authored structural material is used and its collision/traversal behavior remains unchanged

#### Scenario: Removed surface role is supplied
- **WHEN** level data or a runtime resource request supplies a removed shooting-target surface role, material identity or texture
- **THEN** it is rejected or absent rather than mapped to a compatibility texture layer

#### Scenario: Structural material changes independently
- **WHEN** a floor slab is assigned wood-floor and a wall is assigned wallpaper
- **THEN** each uses that appearance with unchanged dimensions, collision kind, tint, and support behavior

### Requirement: Deterministic tiled texture coordinates
Every generated prototype-solid face and terrain triangle SHALL carry finite texture coordinates with a consistent world-space texel density, SHALL map shared surface positions continuously, and SHALL repeat rather than stretch the full texture across differently sized surfaces.

#### Scenario: One planar box face is generated
- **WHEN** a prototype solid is expanded into the two triangles of one face
- **THEN** their shared positions have matching texture coordinates and the complete face has a continuous orientation-correct mapping

#### Scenario: Terrain cell is generated
- **WHEN** one terrain cell is expanded into its two triangles
- **THEN** their shared positions have matching floor texture coordinates derived from world-space horizontal position

#### Scenario: Differently sized surfaces are generated
- **WHEN** a large terrain region and a smaller obstacle face use the same texture role
- **THEN** their texture coordinates preserve the configured world-space scale and the larger surface contains more repetitions

### Requirement: Filtered opaque texture sampling
Supported scene textures SHALL repeat outside normalized coordinates and provide a complete mip chain. Source nearest assets SHALL use nearest magnification and nearest sampling of the nearest mip; the legacy prototype surfaces SHALL retain linear/trilinear sampling. RGB SHALL use sRGB texture interpretation and alpha SHALL remain linear. MASK coverage SHALL compare filtered/mip-sampled alpha times factor alpha with the same authored cutoff at every distance. OPAQUE alpha SHALL NOT introduce transparent surfaces. Texture changes SHALL NOT alter the existing world-scaled UV density on generated surfaces.

#### Scenario: Textured surface recedes from the camera
- **WHEN** a supported scene texture occupies progressively fewer screen pixels
- **THEN** sampling can use progressively smaller mip levels rather than relying only on the full-resolution image

#### Scenario: Texture coordinates exceed one tile
- **WHEN** generated texture coordinates extend outside the normalized texture range
- **THEN** the selected surface texture repeats across the face

#### Scenario: Pixel-art prop is magnified
- **WHEN** a selected chair or phone is viewed close enough to magnify its base-color texels
- **THEN** nearest sampling preserves the authored texel boundaries instead of applying the legacy surface sampler

### Requirement: Textured prototype surface appearance
Every prototype surface SHALL combine its sampled material base color and factor with its authored tint, the authored ambient environment, each currently enabled authored point light, and any active dynamic spot-light contribution without target-specific highlight, dimming, or presentation state.

#### Scenario: Prototype surface is rendered
- **WHEN** a textured prototype solid is visible in a renderable frame
- **THEN** it displays its authored structural material modulated by its authored tint and the bounded point-plus-optional-spot lighting

#### Scenario: Point-light state changes
- **WHEN** one authored point light is disabled by a frame's lighting state
- **THEN** texture sampling, tint, depth visibility, ambient, and other active lights retain their behavior while that point light's contribution is absent

### Requirement: Fixed imported-prop appearance
Imported static placements SHALL preserve finite TEXCOORD_0 values and use their selected packaged base-color material, its factor, alpha mode and sampler through the same bounded scene-lighting calculation as generated geometry. The legacy prototype-chair catalog entry SHALL explicitly retain the previous obstacle-texture appearance. Runtime models outside the supported material profile SHALL fail rather than silently discarding their material inputs.

#### Scenario: Imported prop is rendered
- **WHEN** a fragment from the loaded model is shaded
- **THEN** it samples its selected material at the model-provided coordinates, applies OPAQUE or MASK coverage, and combines surviving color with the existing scene lighting

#### Scenario: Model contains material metadata
- **WHEN** a supported GLB associates its primitive with an embedded base-color texture, factor and sampler
- **THEN** its placement uses those accepted material inputs; only the explicit legacy chair assignment retains the legacy obstacle appearance
