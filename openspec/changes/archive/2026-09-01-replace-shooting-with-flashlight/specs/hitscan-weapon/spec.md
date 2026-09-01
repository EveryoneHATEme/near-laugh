## REMOVED Requirements

### Requirement: Automatic prototype rifle
**Reason**: The primary action now toggles the player flashlight instead of firing a prototype weapon.

**Migration**: Remove rifle trigger sampling and fixed-step weapon state; route captured primary-action press edges to the player flashlight.

### Requirement: Authoritative hitscan shot
**Reason**: The prototype no longer emits shots or resolves weapon hits.

**Migration**: Remove shooting ray construction and closest-hit processing; no replacement weapon query is required.

### Requirement: Bounded rifle recoil
**Reason**: Recoil existed only as feedback for the removed prototype rifle.

**Migration**: Produce camera and flashlight direction from the player's base first-person look without a recoil offset.

