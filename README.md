
# Transit Network Model

Three parts, running in real time:
1. particle filter for vehicle location and speed
2. Kalman filter for transit road network state (speed)
3. travel- and arrival-time predictions for each vehicle/stop combination in the network


### To-do

- Application to run indefinitely
- Use a `Vehicle` object concept with
  - `vector<Particle> (N)`
  - `void update (gtfs::VehiclePosition, gtfs::TripUpdate)` // adjust the position, arrival/departure times etc, trigger particle transitions
  - `void resample (N)` // perform particle filter weighted resample
  - properties `vehicle_id`, `timestamp`, `trip_id`, `route_id`, ...
- And the particles work in memory only
  - `Particle`
    - `void initialize ()`
    - `void transition ()`
    - `void calc_likelihood ()` // uses parent Vehicle
    - `void calc_weight ()`
    - properties `distance`, `velocity`, ..., `likelihood`, `weight`
- Similar concept for network route segments
  - `Segment`
    - `vector<Path> shape` // the GPS coordinates and cumulative distance of segment shape
    - `double speed`
    - `void update ()` // perform Kalman filter update, using particle summaries (?)
- The GTFS information can either be
  - loaded into an SQLite database, or
  - loaded into a MEMORY table via MySQL
- Vehicle state summaries can be written to a file (?)
- Making information available (via server) - road segment speeds + arrival time predictions
  - database (with no foreign key checks, and no transaction?)


(?) best way of collecting vehicle/segment data
- sequentially append speed estimates to `Segment`, then periodically update and clear?
- write to file? (makes keeping history easier?)
