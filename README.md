
# Transit Network Model

Three parts, running in real time:
1. particle filter for vehicle location and speed
2. Kalman filter for transit road network state (speed)
3. travel- and arrival-time predictions for each vehicle/stop combination in the network


### 1. Particle Filter

__IN__: GTFS realtime protobuf feed

__OUT__: (updated) vehicle objects with updated particle states


### 2. Kalman filter

__IN__: particle filter state estimates, road state at time `now - delta`

__OUT__: road state at time `now`

### 3. Predictions

__IN__: particle filter state estimates, road state estimates

__OUT__: ETA to remaining stops along route



****
## Dependencies
- `apt-get install build-essential cmake unzip libboost-all-dev libprotobuf-dev libsqlite3-dev cxxtest sqlite3`
- (optional) Doxygen (for making the Documentation)
- (optional) Google protobuf compiler `protoc`: https://github.com/google/protobuf/blob/master/src/README.md (install with `make protoc`)


## To-do

- Application to run indefinitely
- Use a `Vehicle` object concept with
  - `vector<Particle> (N)`
  - `void update (gtfs::VehiclePosition, gtfs::TripUpdate)`: adjust the position, arrival/departure times etc, trigger particle transitions
  - `void resample (N)`: perform particle filter weighted resample
  - properties `vehicle_id`, `timestamp`, `trip_id`, `route_id`, `position`, `stop_sequence`, `arrival_time`, `departure_time`
- And the particles work in memory only
  - `Particle`
    - `void initialize ()`
    - `void transition ()`
    - `void calc_likelihood ()`: uses parent Vehicle
    - `void calc_weight ()`
    - properties `distance`, `velocity`, `stop_index`, `arrival_time`, `departure_time`, `segment_index`, `queue_time`, `begin_time`, `likelihood`, `weight`
- Similar concept for network route segments
  - `Segment`
    - `vector<Path> shape`: the GPS coordinates and cumulative distance of segment shape
    - `double speed`
    - `void update ()`: perform Kalman filter update, using particle summaries (?)
- The GTFS information can either be
  - loaded into an SQLite database, or
  - loaded into a MEMORY table via MySQL
- Vehicle state summaries can be written to a file (?)
- Making information available (via server) - road segment speeds + arrival time predictions
  - database (with no foreign key checks, and no transaction?)


(?) best way of collecting vehicle/segment data
- sequentially append speed estimates to `Segment`, then periodically update and clear?
- write to file? (makes keeping history easier?)


## Project Structure

- `docs`: documentation (HTML and LaTeX)
- `gps`: a library containing methods for dealing with GPS coordinates
- `gtfs`: a library with GTFS object classes, and methods for modeling them
    - `Vehicle`: Class representing a physical vehicle
    - `Particle`: Class representing a single vehicle state estimate
    - `Segment`: Class representing a road segment
- `include`: header files for programs
- `protobuf`: GTFS Realtime protobuf description and classes
- `src`
  - `transit_network_model.cpp`: mostly just a wrapper for `while (TRUE) { ... }`
  - `load_gtfs.cpp`: a program that imports the latest GTFS data and segments it
