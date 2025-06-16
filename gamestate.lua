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
    levelTimeLimit = 0
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
  state.invulnerable = true
  state.invulnerabilityTimer = constants.INVULNERABILITY_DURATION
end

-- Reset level timer
function gamestate.resetLevelTimer(state, timeLimit)
  state.timeLeft = timeLimit
  state.levelTimeLimit = timeLimit
end

return gamestate
