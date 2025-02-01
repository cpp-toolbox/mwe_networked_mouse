# mwe_networked_mouse
A system that keeps mouse feeling fast on a client server setup 

# question
Is having more mouse updates than the frame rate a beneificial thing? I think this is somewhat true, for the sake of argument suppose your screen was 1Hz and you only updated your view by sampling the mouse at the same rate of 1 Hz, then if you saw a player on the screen, then during the next one second you try to move the mouse to the enemy only one mouse delta (dx, dy) will be used to change your look direction. If instead during that one second you were able to process the mouse deltas multiple times I have a feeling it would be more accurate. At the same time this idea seems a little useless, in order to have time to do this "sub frame" mouse processing you would need extra time per frame to spend doing this, so this might only be possible on higher end machines.

I thought about this more and I think the point is even more subtle, suppose you see the enemy at time t = 0 and your mouse has a sensitivity of 30cm/360, suppose you must rotate your camera 30 degrees to the right to hit the shot on the enemy, since 30 = 1/12 of 360, then you must move your mouse a distnace of 30cm/12 to the right to get the cursor on the enemy, suppose that you can do that before the next game tick occurs, now consider the situation where the mouse delta is only computed once at the start of each tick, by the linearity of mouse deltas idea, then it doesn't matter if you sample the mouse a hundered times or one time as it the deltas will add up to the same thing before the next frame. The reason why that actually doesn't fix the problem is that it's not always going to be the case that you can get your mouse to the target position on the mouse pad that will result in the correct in-game rotation before the next tick, without bogging myself down in the details, having more samples probably means you can either get your desired yaw pitch state at an earlier position in time causing you to be able to hit the shot before the player reacts or something like that...

# idea
Suppose the client samples at a rate of 512hz, because that is the frequency of their monitor, we can get new mouse updates at that rate, so in one second we will get 512 mouse updates of the form (dx1, dy1), (dx2, dy2) , ..., (dx512, dy512), based on how the camera is updated for each mouse update first get yaw pitch deltas by doing `dyaw = dx1 * sens` and `dpitch = dy1 * sens`, then we update yaw via `yaw = yaw + dyaw` and `pitch = pitch + dpitch` therefore yaw and pitch overtime will be a large sum of sequential yaw pitch deltas, in otherwords after 1 second `yaw = sens * (dx1 + dx2 + ... + dx512)` and similarly for `pitch`. 

Focusing only on `yaw` now because for `pitch` the same idea applies we can see that `yaw` is linear, meaning that we can instead write it as `yaw = sens * (dx1 + ... + dx256) + sens * (dx257 + ... + dx512)`. Additionally if we had only sampled our mouse at a rate of 2Hz, then there would be two mouse deltas `(dx'1, dy'1)` and `(dx'2, dy'2)`, but then note that since the mouse travelled the same distance and the only difference was in the way it was sampled then it's most likely true that `dx'1 = (dx1 + ... + dx256)` and also that `dx'2 = (dx257 + ... + dx512)`, in that case I believe that this means that if sample and update our yaw pitch at a rate of 512 on the client, but then send it out to the server by batching together, the resulting yaw pitch will be the same, so potentially this is a good solution to our problem?

# dev notes
The system to be implemented is a client prediction and server reconciliation architecture, it consists of: 
- A client
- A server

## client
It will consist of a system which processes mouse inputs, which are received using glfw which gives us as input the (x, y) coordinates of the mouse, note that regularly these values would be confined to the rectangle (0, 0) - (1080, 1920) or whatever resolution the monitor is, but when the mouse is "grabbed" ie the mouse cursor is not visible on the screen, the values may technically become negative and have no restriction on their values.

A client also consists of a monitor and a mouse, both of which have their own poll rates, for our purposes, suppose the monitor is 144-256hz monitor and that we want to be able to provide frames up to and exceeding the the monitors resolution, note that if we only processed mouse inputs at a rate of 60hz then when looking around, the game would not be feeling smooth in the sense that the view angle of the player would only change at a rate of 60hz, thus it's important to be able to process mouse inputs at a rate which is equal or exceeding the monitor refresh rate. 

Also there is a rate at which the client will send the keyboard and mouse updates to the server. 

## server
At the same time, we have a server that simulates the game world at its own rate, suppose that it updates the physics world at 60hz, the physics world is what simulates explosions, interactions between players and movement changes, the output from the server is the position of the player, what their view angles are (yaw pitch) etc... 

*Note that there is a [mouse submodule](https://github.com/cpp-toolbox/mouse) which includes a function to to process these mouse (x, y) coordinates and convert them to yaw pitch values.*

# terminology
When talking about this entire client server system to speed things up we have these abbreviations to help speed up communication:
- `network_space`: updates travel between client and server, there are four places information can be at any moment it can either be on the client, it can be travelling from the client to the server, it can be on the server, or it can be travelling from the server to client, which we denote by `|C|`, `|C|->|S|`, `|S|`, `|S|->|C|`
- `tick`: a tick generally refers to the action of a system performing the logic contained in one loop
- `tick_rate`: the rate at which a specific system runs at, note that we prefer to be specific eg `client_mouse_freq` instead of just saying the `tick_rate` of the mouse.
- `km_update`: mouse x and y position along with keys currently pressed on the keyboard and mouse
- `client_monitor_freq`: the refresh rate mesured in Hz of the clients monitor
- `client_mouse_freq`: the poll rate of the mouse
- `client_km_update_avg_bundle_size`: as mentioned before multiple km_updates may be generated between two send events, these events get bundled together and sent in the next tick, this value represents the average number of km_updates which are being sent out per km_update send event.
- `client_keyboard_freq`: the poll rate of the keyboard
- `client_km_update_send_freq`: the rate at which the client sends km_updates to the server
- `server_simulation_freq`: the rate at which the servers simulation runs
- `server_game_update_send_freq`: the ratw at which the server sends game updates to the clients
- `current_game_state` on tick `n`: the game state on the server on tick `n` where  

# facts
- `Authorative Server (AS)`: the server is authorative, if we don't use the game updates to update the clients view, and only do client side predictions, the view can get entirely off the rails, and where you're currently looking could be 90deg away from your actual look direction making it impossible to aim. Thus we need to make sure that the client is sychronized to the reality of the server.
- `min_subsystem_rate_rule (msrr)`: if you have subsystems each of which create information, and that information is passed into the next subsystem and if each subsystem processes data at a certain rate, then the rate at which information can be processed through the entire system is the minimum processing rate over all subsystems, if no modifications are made to the system.
- it is a guarentee that `client_mouse_freq > client_km_update_send_freq`, this means that there will be multiple mouse updates between km_updates being sent out, therefore we need a way to deal with this, here's what valve does: (we should probably do the same because their games feel good)
> The client creates user commands from sampling input devices with the same tick rate that the server is running with. A user command is basically a snapshot of the current keyboard and mouse state. But instead of sending a new packet to the server for each user command, the client sends command packets at a certain rate of packets per second (usually 30). This means two or more user commands are transmitted within the same packet. Clients can increase the command rate with cl_cmdrate. This will increase responsiveness but requires more outgoing bandwidth, too. 
- it is a guarentee that `client_monitor_freq > server_game_update_send_freq` therefore by `msrr` if we don't make any modifictions, our monitor would only see changes at a rate less than or equal to `server_game_update_send_freq`, which is bad, because we are no longer using all possible frames available to us on the monitor

# solution
The solution to solve `msrr` in the client server model as specified above is to render mouse inputs at a rate execeeding the monitor refresh rate, on the client side, with the following extremely important modifiction
* for each km_update it is enumerated, which we can call it's `id` when a bundle of km_updates arrive on the server, they are processed in order and the when the server sends a game update out, it also sends out the last `id` that has been processed by the server
* when the game update is received by the client, it immediately replaces the current game state, whatever it may be with what the server told us it is, but while things were flying over the network, the client has processed more km_updates in the mean time, and thus it's moved forward with things, perhaps changing the look direction, so if you just slam the new update onto the client, their view "rubberbands" back to where it was before causing a jerky effect, so we need reconcile this, the way that reconciliation is done is by collecting all km_updates that occurred after the one last processed on the server, and re-apply them all in the same frame that the game update is received to "get us back to" where we were looking, most of the time this look direction is the same as what we predicted, but sometimes it might not be, meaning that the client and server simulation have diverged a bit, and it has been corrected for, which is good and hopefully if there is a delta it is not large.
* Also at this point it becomes important to think about the `game_state` for each `km_update` and a `game_state` we say that it is applied if the `km_update` has already been used to update the `game_state`, otherwise it is not yet applied

# terminology again
* `predicted_km_update (pkm_update)`: a `km_update` on the client which is applied on the current client game state, and possibly applied to the server game state but importantly, we have not run the reconciliation process on it yet, therefore this `km_packet` is in any of the `|C|`, `|C|->|S|`, but not yet `|C|->|S|->|C|`
* `reconciled_km_update (rkm_update)`: a `km_update` which has been applied on the server, and then reconciled against on the server, ie this `km_update` has completed the travel path `|C|->|S|->|C|`
* `last_predicted_client_game_state`: the game state right before the reconciliation algorithm runs
* `reconciled_client_game_state`: the game state after the reconcilation algorithm runs
* `game_state_delta`: Given two game states, this is a function which returns a real number representing how much the two game states differ by, one way of doing this by measuring the distance between all of the same objects, and their look directions.
* `prediction_delta`: `game_state_delta(last_predicted_client_game_state, reconciled_client_game_state)`
* `pkm_update_avg_bundle_size`: Every time we receive a game update, we have to re-apply `pkm_updates` over time we can measure on average how many `pkm_updates` are getting repplied evertime a game update comes in from the server, this metric is important because the larger this value is the higher (potientially) the `prediction_delta`

# pseudocode
```
when a game update (gu) is received on the client:
  set_state(gu)
  for each pkm_update such that pkm_update.id > gu.id:
    update_state(pkm_update)
```
note the above looks simple but making sure everything is operating correctly is hard.
    
