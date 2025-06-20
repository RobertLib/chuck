local constants = {}

-- Screen dimensions
constants.SCREEN_WIDTH = 800
constants.SCREEN_HEIGHT = 600

-- Physics constants
constants.GRAVITY = 500
constants.CRATE_GRAVITY = 200 -- Slower gravity for crates (40% of player gravity)
constants.JUMP_SPEED = -300
constants.PLAYER_SPEED = 150
constants.ENEMY_SPEED = 50

-- Collision detection constants
constants.COLLISION_TOLERANCE = 2
constants.LADDER_EXPAND_TOLERANCE = -1
constants.SUPPORT_TOLERANCE = 2

-- Animation constants
constants.ANIM_SPEED = 8 -- frames per second for animations

-- UI constants
constants.STATUS_BAR_HEIGHT = 35
constants.STATUS_BAR_PADDING = 15
constants.INVULNERABILITY_DURATION = 2.0
constants.LEVEL_COMPLETE_DISPLAY_TIME = 2.0

-- Game object constants
constants.PLAYER_WIDTH = 20
constants.PLAYER_HEIGHT = 47
constants.ENEMY_WIDTH = 18
constants.ENEMY_HEIGHT = 25
constants.COLLECTIBLE_WIDTH = 15
constants.COLLECTIBLE_HEIGHT = 20
constants.CRATE_SIZE = 25
constants.SPIKE_WIDTH = 30
constants.SPIKE_HEIGHT = 15

-- Animation timing constants
constants.WALK_CYCLE_SPEED = 5
constants.CLIMB_CYCLE_SPEED = 3
constants.COLLECTIBLE_ANIM_SPEED = 3
constants.ENEMY_ANIM_SPEED = 6
constants.BREATHING_SPEED = 3

-- Physics constants
constants.CRATE_PUSH_SPEED_FACTOR = 0.4
constants.VELOCITY_THRESHOLD = 10

-- Visual constants
constants.GLOW_RADIUS = 12
constants.SPARKLE_COUNT = 3
constants.SPARKLE_DISTANCE = 15

-- Pickup animation constants
constants.PICKUP_ANIMATION_DURATION = 1.5
constants.PICKUP_SCALE_MAX = 3.5
constants.PICKUP_FADE_SPEED = 2.0

-- Player death animation constants
constants.PLAYER_DEATH_PARTICLES_DURATION = 2

return constants
