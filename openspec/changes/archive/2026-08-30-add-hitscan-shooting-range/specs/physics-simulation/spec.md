## ADDED Requirements

### Requirement: Backend-neutral static ray query
The physics boundary SHALL accept a finite ray origin, a finite non-zero direction, and a finite positive maximum distance, and SHALL report either no static hit or the closest intersected prototype solid's stable index and distance. The query SHALL consider the same static collision geometry used by player movement, SHALL NOT advance simulation, and SHALL NOT expose physics-library body, vector, query, or result types.

#### Scenario: Ray intersects multiple static solids
- **WHEN** a valid ray segment crosses more than one prototype static body
- **THEN** the query reports the stable solid index and distance of only the closest intersection

#### Scenario: Ray misses static collision
- **WHEN** a valid ray segment reaches its maximum distance without intersecting a prototype static body
- **THEN** the query reports no hit

#### Scenario: Ray query is invalid
- **WHEN** the origin or direction is non-finite, the direction has zero length, or maximum distance is non-finite or non-positive
- **THEN** the query rejects the request without advancing or mutating physics state

#### Scenario: Physics boundary is inspected
- **WHEN** the static ray-query API and its consumers are inspected
- **THEN** they exchange only engine-owned scalar, vector, and static-solid index values

