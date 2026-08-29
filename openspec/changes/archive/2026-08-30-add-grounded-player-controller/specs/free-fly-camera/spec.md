## REMOVED Requirements

### Requirement: Perspective camera frame
**Reason**: The temporary standalone free-fly camera is replaced by the grounded player's first-person camera, which retains the backend-neutral perspective frame contract.

**Migration**: Construct camera frames through the player controller's position, look orientation, stance eye height, and current framebuffer aspect.

### Requirement: Free-fly navigation
**Reason**: Unconstrained translation, vertical flight, and passage through geometry conflict with the physical FPS player now required by the prototype.

**Migration**: Route FPS movement actions to grounded player movement; Space becomes a grounded jump and Left Control becomes hold-to-crouch.

### Requirement: Prototype cursor capture
**Reason**: Cursor capture remains required but becomes part of the grounded player capability instead of the removed free-fly camera.

**Migration**: Preserve release and recapture behavior through the player controller's runtime-owned input policy.
