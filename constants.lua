local constants = {}

-- Development mode flag - set to false for production builds
constants.DEV_MODE = true

-- Game version
constants.VERSION = "v1.0"

-- Screen dimensions
constants.SCREEN_WIDTH = 800
constants.SCREEN_HEIGHT = 600

-- Physics constants
constants.GRAVITY = 500
constants.CRATE_GRAVITY = 200 -- Slower gravity for crates (40% of player gravity)
constants.JUMP_SPEED = -300
constants.PLAYER_SPEED = 150
constants.ENEMY_SPEED = 50

-- Fall damage constants
constants.FALL_DAMAGE_THRESHOLD = 300  -- Minimum fall speed to cause damage
constants.MAX_SAFE_FALL_DISTANCE = 250 -- Maximum safe fall distance in pixels

-- Collision detection constants
constants.COLLISION_TOLERANCE = 2
constants.LADDER_EXPAND_TOLERANCE = -1
constants.SUPPORT_TOLERANCE = 2

-- Animation constants
constants.ANIM_SPEED = 8 -- frames per second for animations

-- UI constants
constants.STATUS_BAR_HEIGHT = 35
constants.STATUS_BAR_PADDING = 15
constants.INVULNERABILITY_DURATION = 2.0    -- Duration of invulnerability after taking damage
constants.LEVEL_COMPLETE_DISPLAY_TIME = 2.0 -- How long to display level complete message

-- Game object dimensions
constants.PLAYER_WIDTH = 20
constants.PLAYER_HEIGHT = 47
constants.ENEMY_WIDTH = 18
constants.ENEMY_HEIGHT = 25
constants.COLLECTIBLE_WIDTH = 15
constants.COLLECTIBLE_HEIGHT = 20
constants.LIFE_POWERUP_WIDTH = 18
constants.LIFE_POWERUP_HEIGHT = 18
constants.CRATE_SIZE = 25
constants.SPIKE_WIDTH = 25
constants.SPIKE_HEIGHT = 15

-- Bat constants
constants.BAT_WIDTH = 16
constants.BAT_HEIGHT = 12
constants.BAT_SPEED = 120
constants.BAT_SPAWN_INTERVAL = 6.0   -- Time between bat spawns (seconds)
constants.BAT_SPAWN_HEIGHT_MIN = 50  -- Minimum height for bat spawn
constants.BAT_SPAWN_HEIGHT_MAX = 400 -- Maximum height for bat spawn
constants.BAT_SPEED_VARIATION = 20   -- ±20 speed variation
constants.BAT_SPAWN_CHANCE = 0.5     -- 50% chance to spawn from left vs right

-- Animation timing constants
constants.WALK_CYCLE_SPEED = 5       -- Animation speed for walking
constants.CLIMB_CYCLE_SPEED = 3      -- Animation speed for climbing
constants.COLLECTIBLE_ANIM_SPEED = 3 -- Animation speed for collectibles
constants.ENEMY_ANIM_SPEED = 6       -- Animation speed for enemies
constants.BREATHING_SPEED = 3        -- Animation speed for breathing effect

-- Physics constants
constants.CRATE_PUSH_SPEED_FACTOR = 0.4 -- Speed reduction when pushing crates
constants.VELOCITY_THRESHOLD = 10       -- Minimum velocity threshold for physics

-- Visual constants
constants.GLOW_RADIUS = 12      -- Radius of glow effects
constants.SPARKLE_COUNT = 3     -- Number of sparkles around objects
constants.SPARKLE_DISTANCE = 15 -- Distance of sparkles from object center

-- Pickup animation constants
constants.PICKUP_ANIMATION_DURATION = 1.5 -- Duration of pickup animation
constants.PICKUP_SCALE_MAX = 3.5          -- Maximum scale during pickup animation
constants.PICKUP_FADE_SPEED = 2.0         -- Speed of fade out effect

-- Player death animation constants
constants.PLAYER_DEATH_PARTICLES_DURATION = 2 -- Duration of death particles (seconds)

-- Fireball constants
constants.FIREBALL_WIDTH = 8
constants.FIREBALL_HEIGHT = 8
constants.FIREBALL_SPEED = 180                -- Speed of fireballs
constants.FIREBALL_BOUNCE_SPEED = 150         -- Speed after bouncing
constants.FIREBALL_BOUNCE_DAMPING = 0.7       -- Velocity reduction after bounce
constants.FIREBALL_LIFETIME = 6.0             -- How long fireballs exist (seconds)
constants.FIREBALL_ANIM_SPEED = 8             -- Animation speed for fireballs
constants.ENEMY_FIREBALL_CHANCE = 0.25        -- Chance per second to throw fireball when seeing player
constants.ENEMY_FIREBALL_COOLDOWN = 3.0       -- Minimum time between fireballs
constants.ENEMY_SIGHT_RANGE = 200             -- How far enemy can see player
constants.ENEMY_SIGHT_VERTICAL_TOLERANCE = 50 -- Vertical tolerance for line of sight

-- Enemy ladder behavior constants
constants.ENEMY_LADDER_CHANCE = 0.3            -- 30% chance to climb ladder when encountered
constants.ENEMY_CLIMB_SPEED = 30               -- Speed when climbing ladders
constants.ENEMY_LADDER_CHECK_INTERVAL = 1.0    -- How often to check for ladders (seconds)
constants.ENEMY_LADDER_CENTER_SPEED = 80       -- Speed of centering on ladder horizontally
constants.ENEMY_PLATFORM_TRANSITION_SPEED = 60 -- Speed of transitioning onto platform
constants.ENEMY_CLIMB_DOWN_CHANCE = 0.4        -- 40% chance to climb down when on ladder

-- Water bubble constants
constants.BUBBLE_SPAWN_CHANCE = 0.3 -- 30% chance per water area
constants.BUBBLE_MIN_SPEED = 20     -- Minimum bubble rise speed
constants.BUBBLE_MAX_SPEED = 50     -- Maximum bubble rise speed
constants.BUBBLE_MIN_SIZE = 2       -- Minimum bubble size
constants.BUBBLE_MAX_SIZE = 5       -- Maximum bubble size
constants.BUBBLE_MIN_LIFE = 3       -- Minimum bubble lifetime (seconds)
constants.BUBBLE_MAX_LIFE = 5       -- Maximum bubble lifetime (seconds)
constants.BUBBLE_MIN_ALPHA = 0.3    -- Minimum bubble transparency
constants.BUBBLE_MAX_ALPHA = 0.7    -- Maximum bubble transparency

-- Stalactite constants
constants.STALACTITE_WIDTH = 5
constants.STALACTITE_HEIGHT = 9
constants.STALACTITE_DETECTION_RANGE = 100 -- How far below to detect player
constants.STALACTITE_FALL_DELAY = 0.25     -- Delay before falling after detection (seconds)
constants.STALACTITE_FALL_SPEED = 150      -- Speed when falling (reduced from 300)
constants.STALACTITE_FALL_GRAVITY = 400    -- Gravity acceleration when falling (reduced from 800)
constants.STALACTITE_DEBRIS_DURATION = 3.0 -- How long debris remains visible (seconds)

-- Circular saw constants
constants.CIRCULAR_SAW_WIDTH = 30
constants.CIRCULAR_SAW_HEIGHT = 30
constants.CIRCULAR_SAW_ROTATION_SPEED = 5.0  -- Radians per second when rotating
constants.CIRCULAR_SAW_ROTATE_TIME_MIN = 2.0 -- Minimum rotation time (seconds)
constants.CIRCULAR_SAW_ROTATE_TIME_MAX = 4.0 -- Maximum rotation time (seconds)
constants.CIRCULAR_SAW_PAUSE_TIME_MIN = 1.0  -- Minimum pause time (seconds)
constants.CIRCULAR_SAW_PAUSE_TIME_MAX = 3.0  -- Maximum pause time (seconds)

-- Crumbling platform particle constants
constants.CRUMBLE_PARTICLE_COUNT = 8            -- Number of particles when platform crumbles
constants.CRUMBLE_PARTICLE_VELOCITY_RANGE = 100 -- ±50 horizontal velocity
constants.CRUMBLE_PARTICLE_MIN_VELOCITY_Y = 20  -- Minimum vertical velocity for particles
constants.CRUMBLE_PARTICLE_MAX_VELOCITY_Y = 70  -- Maximum vertical velocity for particles
constants.CRUMBLE_PARTICLE_MIN_SIZE = 2         -- Minimum particle size
constants.CRUMBLE_PARTICLE_MAX_SIZE = 6         -- Maximum particle size
constants.CRUMBLE_PARTICLE_MIN_LIFE = 0.5       -- Minimum particle lifetime (seconds)
constants.CRUMBLE_PARTICLE_MAX_LIFE = 1.5       -- Maximum particle lifetime (seconds)
constants.CRUMBLE_SHAKE_INTENSITY = 0.3         -- Screen shake intensity when crumbling

return constants
