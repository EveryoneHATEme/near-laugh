## REMOVED Requirements

### Requirement: Backend-neutral static ray query
**Reason**: The only consumer was the removed prototype hitscan rifle, and no current gameplay requirement needs a physics ray query.

**Migration**: Remove the static-ray request/result boundary and its shooting-range consumer; retain the existing static collision used by player movement.

