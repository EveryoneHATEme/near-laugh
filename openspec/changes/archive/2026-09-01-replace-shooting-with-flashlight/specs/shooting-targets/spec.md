## REMOVED Requirements

### Requirement: Concrete shooting-range targets
**Reason**: The prototype plates remain scene geometry but no longer own shooting-target gameplay state.

**Migration**: Remove target descriptions, starting health, and runtime target-state construction while retaining the authored solids, texture role, and collision.

### Requirement: Target hit and destruction
**Reason**: Removing shooting also removes damage and target destruction behavior.

**Migration**: Treat every plate as immutable visible and collidable prototype geometry with no mutable health.

### Requirement: Target presentation feedback
**Reason**: Hit highlighting and destroyed dimming have no producer after weapon and damage removal.

**Migration**: Render plates with their ordinary authored textured and lit appearance.

