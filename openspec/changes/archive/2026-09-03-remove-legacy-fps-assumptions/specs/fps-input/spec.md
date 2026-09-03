## REMOVED Requirements

### Requirement: FPS action snapshot
**Reason**: The FPS-named contract encodes the obsolete shooter direction and is replaced by the `player-input` capability.

**Migration**: Update every consumer to the player-input contract; no compatibility type or alias is retained.

### Requirement: Default FPS controls
**Reason**: Default controls remain current prototype behavior but no longer belong to an FPS-named capability.

**Migration**: Use the `Default player controls` requirement in `player-input`; no compatibility requirement is retained.

### Requirement: First-person look delta
**Reason**: The behavior moves unchanged to the `player-input` capability so the obsolete capability can be removed completely.

**Migration**: Use the corresponding `player-input` requirement.

### Requirement: Platform-independent input contract
**Reason**: The platform boundary moves to the replacement `player-input` capability under neutral terminology.

**Migration**: Update consumers and tests to the replacement capability; no FPS-named compatibility surface is retained.
