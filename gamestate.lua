local constants = require("constants")
local collision = require("collision")
local audio = require("audio")

local gamestate = {}

-- Initialize game state
function gamestate.init()
  return {
    currentState = "menu", -- Add menu state
    player = {},
    platforms = {},
    ladders = {},
    collectibles = {},
    lifePowerups = {},
    enemies = {},
    crates = {},
    spikes = {},
    stalactites = {},
    circularSaws = {},
    crushers = {},
    crumblingPlatforms = {},
    movingPlatforms = {},
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
    paused = false,
    -- Menu specific properties
    menuSelectedOption = 1,
    menuOptions = { "Start Game", "Fullscreen", "Quit" },
    hasActiveGame = false, -- Track if there's a game in progress
    isFullscreen = false,  -- Track fullscreen state
  }
end

-- Reset game state
function gamestate.reset(state)
  state.currentState = "menu"
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
  state.menuSelectedOption = 1
  state.hasActiveGame = false
  -- Don't reset fullscreen state on game reset
  gamestate.updateMenuOptions(state)
end

-- Check if spawn position is safe (no immediate dangers)
function gamestate.isSpawnPositionSafe(state, x, y)
  local spawnArea = {
    x = x,
    y = y,
    width = constants.PLAYER_WIDTH,
    height = constants.PLAYER_HEIGHT
  }

  -- Check for spikes at spawn position
  for _, spike in ipairs(state.spikes) do
    if collision.checkCollision(spawnArea, spike) then
      return false, "spike"
    end
  end

  -- Check for enemies at spawn position
  for _, enemy in ipairs(state.enemies) do
    if collision.checkCollision(spawnArea, enemy) then
      return false, "enemy"
    end
  end

  -- Check for water at spawn position
  for _, waterArea in ipairs(state.water) do
    if collision.checkCollision(spawnArea, waterArea) then
      return false, "water"
    end
  end

  return true, nil
end

-- Reset player position with safety check
function gamestate.resetPlayerPositionSafe(state)
  local defaultX, defaultY = 50, 500

  -- Check if default position is safe
  local isSafe, danger = gamestate.isSpawnPositionSafe(state, defaultX, defaultY)

  if not isSafe then
    print("Warning: Default spawn position is not safe (danger: " .. tostring(danger) .. ")")
    -- Try to find a safe position nearby
    local foundSafe = false
    for offsetX = -100, 100, 25 do
      for offsetY = -50, 50, 25 do
        local testX = defaultX + offsetX
        local testY = defaultY + offsetY

        -- Make sure position is within screen bounds
        if testX >= 0 and testX + state.player.width <= constants.SCREEN_WIDTH and
            testY >= 0 and testY + state.player.height <= constants.SCREEN_HEIGHT then
          local safePosCheck, _ = gamestate.isSpawnPositionSafe(state, testX, testY)
          if safePosCheck then
            defaultX = testX
            defaultY = testY
            foundSafe = true
            print("Found safe spawn position at: " .. testX .. ", " .. testY)
            break
          end
        end
      end
      if foundSafe then break end
    end

    if not foundSafe then
      print("Warning: Could not find completely safe spawn position, using default with extended invulnerability")
    end
  end

  -- Set player position
  state.player.x = defaultX
  state.player.y = defaultY
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

  -- Extended invulnerability if spawn wasn't completely safe
  state.invulnerable = true
  if not isSafe then
    state.invulnerabilityTimer = constants.INVULNERABILITY_DURATION * 2 -- Double invulnerability time
  else
    state.invulnerabilityTimer = constants.INVULNERABILITY_DURATION
  end
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

function gamestate.checkWinCondition(gameState, levelCount)
  local allCollectiblesCollected = true
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      allCollectiblesCollected = false
      break
    end
  end

  if allCollectiblesCollected and not gameState.showingLevelComplete then
    if gameState.level < levelCount then
      -- Start showing level complete message
      gameState.showingLevelComplete = true
      gameState.levelCompleteTimer = constants.LEVEL_COMPLETE_DISPLAY_TIME
      audio.playLevelComplete() -- Play level complete sound
    else
      -- Final victory
      gameState.won = true
      audio.playLevelComplete() -- Play victory sound
    end
  end
end

-- Menu functions
function gamestate.updateMenuOptions(state)
  local fullscreenText = state.isFullscreen and "Windowed" or "Fullscreen"
  if state.hasActiveGame then
    state.menuOptions = { "Resume Game", "New Game", fullscreenText, "Quit" }
  else
    state.menuOptions = { "Start Game", fullscreenText, "Quit" }
  end
  -- Reset selection to first option when menu changes
  state.menuSelectedOption = 1
end

function gamestate.navigateMenuUp(state)
  if state.currentState == "menu" then
    state.menuSelectedOption = state.menuSelectedOption - 1
    if state.menuSelectedOption < 1 then
      state.menuSelectedOption = #state.menuOptions
    end
  end
end

function gamestate.navigateMenuDown(state)
  if state.currentState == "menu" then
    state.menuSelectedOption = state.menuSelectedOption + 1
    if state.menuSelectedOption > #state.menuOptions then
      state.menuSelectedOption = 1
    end
  end
end

function gamestate.selectMenuOption(state)
  if state.currentState == "menu" then
    if state.hasActiveGame then
      -- Menu with active game: Resume Game, New Game, Fullscreen/Windowed, Quit
      if state.menuSelectedOption == 1 then -- Resume Game
        state.currentState = "playing"
        return "resume_game"
      elseif state.menuSelectedOption == 2 then -- New Game
        state.currentState = "playing"
        return "new_game"
      elseif state.menuSelectedOption == 3 then -- Fullscreen/Windowed
        return "toggle_fullscreen"
      elseif state.menuSelectedOption == 4 then -- Quit
        return "quit"
      end
    else
      -- Default menu: Start Game, Fullscreen/Windowed, Quit
      if state.menuSelectedOption == 1 then -- Start Game
        state.currentState = "playing"
        state.hasActiveGame = true
        return "start_game"
      elseif state.menuSelectedOption == 2 then -- Fullscreen/Windowed
        return "toggle_fullscreen"
      elseif state.menuSelectedOption == 3 then -- Quit
        return "quit"
      end
    end
  end
  return nil
end

function gamestate.pauseToMenu(state)
  state.currentState = "menu"
  gamestate.updateMenuOptions(state)
end

function gamestate.startGame(state)
  state.currentState = "playing"
  state.hasActiveGame = true
end

function gamestate.toggleFullscreen(state)
  state.isFullscreen = not state.isFullscreen

  local success
  if state.isFullscreen then
    -- Try exclusive fullscreen first with exact resolution
    success = love.window.setMode(constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT, {
      fullscreen = true,
      fullscreentype = "exclusive",
      resizable = false,
      vsync = true
    })

    -- If exclusive doesn't work, fallback to desktop fullscreen
    if not success then
      print("Exclusive fullscreen failed, trying desktop fullscreen")
      success = love.window.setMode(0, 0, {
        fullscreen = true,
        fullscreentype = "desktop",
        resizable = false,
        vsync = true
      })
    end
  else
    -- Set windowed mode with 800x600 resolution
    success = love.window.setMode(constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT, {
      fullscreen = false,
      resizable = false,
      vsync = true
    })
  end

  if not success then
    -- If mode change failed, revert the state
    state.isFullscreen = not state.isFullscreen
    print("Failed to change fullscreen mode")
  end

  -- Update menu options to reflect the change
  gamestate.updateMenuOptions(state)
end

return gamestate
