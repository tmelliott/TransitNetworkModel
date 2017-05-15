
# Transit Network Model

Three parts, running in real time:
1. particle filter for vehicle location and speed
2. Kalman filter for transit road network state (speed)
3. travel- and arrival-time predictions for each vehicle/stop combination in the network


### To-do

- Application to run indefinitely
- Use a `Vehicle` object concept with
  - `vector<Particle> (N)`
  - `update (gtfs::VehiclePosition, gtfs::TripUpdate)`
- And the particles work in memory only
  - `Particle`
    - `initialize ()`
    - `transition ()`
    - `likelihood ()` // uses parent Vehicle
