local constants = require("constants")

local gamestate = {}

-- Initialize game state
function gamestate.init()
  return {
    player = {},
    platforms = {},
    ladders = {},
    collectibles = {},
    enemies = {},
    crates = {},
    spikes = {},
    crumbling_platforms = {},
    moving_platforms = {},
    water = {},
    bats = {},
    fireballs = {},
    batSpawnTimer = 0,
    score = 0,
    level = 1,
    lives = 3,
    gameOver = false,
    won = false,
    levelCompleteTimer = 0,
    showingLevelComplete = false,
    invulnerable = false,
    invulnerabilityTimer = 0,
    globalTime = 0, -- For animations and shader
    backgroundShader = nil,
    timeLeft = 0,
    levelTimeLimit = 0,
    paused = false
  }
end

-- Reset game state
function gamestate.reset(state)
  state.gameOver = false
  state.won = false
  state.showingLevelComplete = false
  state.levelCompleteTimer = 0
  state.globalTime = 0
  state.score = 0
  state.level = 1
  state.lives = 3
  state.invulnerable = false
  state.invulnerabilityTimer = 0
  state.timeLeft = 0
  state.levelTimeLimit = 0
  state.paused = false
end

-- Reset player position
function gamestate.resetPlayerPosition(state)
  state.player.x = 50
  state.player.y = 500
  state.player.velocityX = 0
  state.player.velocityY = 0
  state.player.onGround = false
  state.player.onLadder = false
  state.player.animState = "idle"
  state.player.animTime = 0
  state.player.facing = 1
  -- Reset respawn delay state
  state.player.isWaitingToRespawn = false
  state.player.respawnTimer = 0
  -- Reset fall damage tracking
  state.player.fallStartY = 0
  state.player.isFalling = false
  state.player.maxFallSpeed = 0
  state.invulnerable = true
  state.invulnerabilityTimer = constants.INVULNERABILITY_DURATION
end

-- Reset level timer
function gamestate.resetLevelTimer(state, timeLimit)
  state.timeLeft = timeLimit
  state.levelTimeLimit = timeLimit
end

-- Toggle pause state
function gamestate.togglePause(state)
  state.paused = not state.paused
end

return gamestate
