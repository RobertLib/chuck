local constants = require("constants")
local gamestate = require("gamestate")
local levels = require("levels")
local player = require("player")
local enemies = require("enemies")
local bats = require("bats")
local crates = require("crates")
local collectibles = require("collectibles")
local ui = require("ui")
local background = require("background")
local input = require("input")
local levelEditor = constants.DEV_MODE and require("leveleditor") or nil
local transition = require("transition")
local particles = require("particles")
local water = require("water")
local movingPlatforms = require("moving_platforms")
local conveyorBelts = require("conveyor_belts")
local fireballs = require("fireballs")
local playerDeath = require("player_death")
local crumblingPlatforms = require("crumbling_platforms")
local crushers = require("crushers")
local platforms = require("platforms")
local ladders = require("ladders")
local decorations = require("decorations")
local spikes = require("spikes")
local stalactites = require("stalactites")
local circularSaws = require("circular_saws")
local shaders = require("shaders")
local audio = require("audio")

-- Game state
local gameState

function love.load()
  -- Initialize random seed for consistent random behavior
  math.randomseed(os.time())

  love.window.setTitle("Chuck")
  love.window.setMode(constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT, { resizable = false, vsync = true })

  -- Initialize audio system
  audio.init()

  -- Initialize game state
  gameState = gamestate.init()

  -- Initialize fullscreen state based on current window mode
  gameState.isFullscreen = love.window.getFullscreen()

  -- Initialize menu options
  gamestate.updateMenuOptions(gameState)

  -- Load levels from external file
  levels.loadLevels()

  -- Clear any temporary level changes from previous session
  levels.clearTemporaryLevels()

  -- Initialize level editor only in dev mode
  if constants.DEV_MODE and levelEditor then
    levelEditor.init()
  end

  -- Initialize background shader
  gameState.backgroundShader = shaders.initBackgroundShader()
end

-- Helper function to update all game objects
local function updateGameObjects(dt, gameState)
  player.updatePlayer(dt, gameState)
  player.updatePlayerAnimation(dt, gameState)
  enemies.updateEnemies(dt, gameState)
  fireballs.updateFireballs(dt, gameState)
  bats.updateBats(dt, gameState)
  crates.updateCrates(dt, gameState)
  collectibles.updateCollectibles(dt, gameState)

  -- Update moving platforms
  movingPlatforms.update(gameState.movingPlatforms, dt)

  -- Update conveyor belts
  conveyorBelts.update(gameState.conveyorBelts or {}, dt)

  -- Update crumbling platforms
  crumblingPlatforms.update(gameState, dt)

  -- Update crushers
  crushers.update(gameState, dt)

  -- Update stalactites
  stalactites.update(gameState, dt)

  -- Update circular saws
  circularSaws.update(gameState, dt)

  -- Update water system
  water.update(dt, gameState)

  -- Update particles
  particles.update(dt)
end

-- Helper function to check all deadly collisions
local function checkDeadlyCollisions(gameState)
  -- Check water collision - player dies if touching water
  if water.checkCollision(gameState) then
    if not gameState.player.isWaitingToRespawn then
      playerDeath.killPlayerAtCenter(gameState, "water")
    end
  end

  -- Check deadly collisions (return early if player dies)
  if spikes.checkCollision(gameState) then
    return true
  end
  if stalactites.checkCollision(gameState) then
    return true
  end
  if circularSaws.checkCollision(gameState) then
    return true
  end
  if enemies.checkCollision(gameState) then
    return true
  end
  if bats.checkCollision(gameState) then
    return true
  end
  if fireballs.checkCollision(gameState) then
    return true
  end

  -- Check crusher collision
  if crushers.checkPlayerCollision(gameState) then
    if playerDeath.killPlayerAtCenter(gameState, "crusher") then
      -- Create metal crushing particles
      particles.createImpactParticles(
        gameState.player.x + gameState.player.width / 2,
        gameState.player.y + gameState.player.height / 2,
        { 0.7, 0.7, 0.7, 1.0 } -- Gray metallic color
      )
    end
  end

  return false
end

-- Helper function to draw all game objects
local function drawGameObjects(gameState)
  -- Draw circular saws first (under everything else)
  circularSaws.draw(gameState)

  crates.drawCrates(gameState)
  spikes.draw(gameState)
  stalactites.draw(gameState)
  crushers.draw(gameState)

  -- Draw game objects
  decorations.drawDecorations(gameState)
  platforms.draw(gameState)
  crumblingPlatforms.draw(gameState)
  movingPlatforms.draw(gameState.movingPlatforms)
  if gameState.conveyorBelts then
    conveyorBelts.draw(gameState.conveyorBelts)
  end
  ladders.draw(gameState)

  -- Draw collectible items (gems/crystals)
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      collectibles.drawCollectibleItem(collectible)
    elseif collectible.isPickingUp then
      -- Draw pickup animation for collected items
      collectibles.drawPickupAnimation(collectible)
    end
  end

  -- Draw life power-ups
  for _, lifePowerup in ipairs(gameState.lifePowerups) do
    if not lifePowerup.collected then
      collectibles.drawLifePowerupItem(lifePowerup)
    elseif lifePowerup.isPickingUp then
      -- Draw pickup animation for collected life power-ups
      collectibles.drawPickupAnimation(lifePowerup)
    end
  end

  -- Draw water (before enemies and player so they appear on top)
  water.draw(gameState, gameState.globalTime)

  -- Draw enemies
  for _, enemy in ipairs(gameState.enemies) do
    enemies.drawEnemy(enemy)
  end
  bats.drawBats(gameState)

  -- Draw fireballs
  for _, fireball in ipairs(gameState.fireballs) do
    fireballs.drawFireball(fireball)
  end

  -- Draw particles (after player and objects but before UI)
  particles.draw()

  -- Draw player (with blinking effect when invulnerable)
  if not gameState.invulnerable or math.floor(gameState.invulnerabilityTimer * 10) % 2 == 0 then
    player.drawPlayer(gameState)
  end
end

function love.update(dt)
  if levels.loadError then
    return -- Stop update if there is a level loading error
  end

  -- Skip game update if level editor is active (only in dev mode)
  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    return
  end

  -- Skip game update if in menu
  if gameState.currentState == "menu" then
    -- Update global time for menu animations
    gameState.globalTime = gameState.globalTime + dt
    return
  end

  -- Skip game update if paused
  if gameState.paused then
    return
  end

  if gameState.gameOver then
    return
  end

  -- Handle level transition
  if transition.isActive() then
    transition.updateTransition(dt, gameState)
    return -- Don't update game while transition is active
  end

  -- Handle level complete timer
  if gameState.showingLevelComplete then
    gameState.levelCompleteTimer = gameState.levelCompleteTimer - dt
    if gameState.levelCompleteTimer <= 0 then
      -- Start transition to next level instead of immediate switch
      local nextLevel = gameState.level + 1
      transition.startTransition(gameState, nextLevel)
    end
    return -- Don't update game while showing level complete message
  end

  if gameState.won then
    return
  end

  -- Update global time for animations and shaders
  gameState.globalTime = gameState.globalTime + dt

  -- Update level timer
  if gameState.timeLeft > 0 then
    gameState.timeLeft = gameState.timeLeft - dt
    if gameState.timeLeft <= 0 then
      gameState.timeLeft = 0
      -- Time's up - show times up panel and restart level after short delay
      gameState.showingTimesUp = true
      gameState.timesUpTimer = 2.0 -- seconds to show the panel
    end
  end

  -- Handle times up panel and restart level
  if gameState.showingTimesUp then
    gameState.timesUpTimer = gameState.timesUpTimer - dt
    if gameState.timesUpTimer <= 0 then
      gameState.showingTimesUp = false
      -- Restart the same level, keep lives
      levels.createLevel(gameState)
      gameState.player = player.initializePlayer()
    end
    return -- Don't update game while showing times up panel
  end

  updateGameObjects(dt, gameState)

  -- Check collisions with various objects
  collectibles.checkCollision(gameState)

  -- Check deadly collisions (return early if player dies)
  if checkDeadlyCollisions(gameState) then
    return
  end

  gamestate.checkWinCondition(gameState, levels.getLevelCount())

  -- Update invulnerability timer
  if gameState.invulnerable then
    gameState.invulnerabilityTimer = gameState.invulnerabilityTimer - dt
    if gameState.invulnerabilityTimer <= 0 then
      gameState.invulnerable = false
    end
  end
end

function love.draw()
  if levels.loadError then
    ui.drawGameMessages(gameState, levels.getLevelCount(), levels)
    return
  end

  -- If level editor is active, draw editor instead of game (only in dev mode)
  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    levelEditor.draw()
    return
  end

  -- If in menu state, draw menu
  if gameState.currentState == "menu" then
    -- Draw animated background behind menu
    background.draw(gameState)
    ui.drawMainMenu(gameState)
    return
  end

  -- If transition is active, draw transition instead of normal game
  if transition.isActive() then
    transition.drawTransition(gameState)
    -- Still draw UI on top of transition
    ui.drawTopStatusBar(gameState)
    return
  end

  -- Draw animated background
  background.draw(gameState)

  drawGameObjects(gameState)

  -- UI - Top status bar
  ui.drawTopStatusBar(gameState)

  -- Game state messages
  ui.drawGameMessages(gameState, levels.getLevelCount(), levels)
end

function love.keypressed(key)
  -- Global fullscreen toggle (F11 key) - works in any state
  if key == "f11" then
    gamestate.toggleFullscreen(gameState)
    return
  end

  -- Handle level editor key presses first (only in dev mode and not in menu)
  if constants.DEV_MODE and key == "f1" and levelEditor and gameState.currentState ~= "menu" then
    levelEditor.toggle(gameState)
    return
  end

  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    levelEditor.keypressed(key)
    return
  end

  -- Handle pause key press (works only during gameplay)
  if key == "p" and gameState.currentState == "playing" and not gameState.gameOver and not gameState.won then
    gamestate.togglePause(gameState)
    return
  end

  -- Skip other inputs if paused
  if gameState.paused then
    return
  end

  -- Handle transition skip
  if transition.isActive() then
    if key == "space" or key == "return" or key == "escape" then
      transition.skipTransition(gameState)
    end
    return
  end

  input.handleKeyPressed(key, gameState, levels, gamestate)
end

-- Add mouse handling for level editor (only in dev mode)
function love.mousepressed(x, y, button)
  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    levelEditor.mousepressed(x, y, button)
    return
  end

  -- Handle mouse clicks in menu
  if gameState.currentState == "menu" and button == 1 then -- Left mouse button
    if ui.handleMenuClick(gameState, x, y) then
      -- A menu option was clicked, execute the selection
      local action = gamestate.selectMenuOption(gameState)
      if action == "start_game" or action == "new_game" then
        -- Initialize the game when starting new game
        gameState.player = require("player").initializePlayer()
        gamestate.resetPlayerPositionSafe(gameState)
        levels.createLevel(gameState)
      elseif action == "resume_game" then
        -- Just resume the existing game state
        -- No need to reinitialize anything
      elseif action == "toggle_fullscreen" then
        gamestate.toggleFullscreen(gameState)
      elseif action == "quit" then
        love.event.quit()
      end
    end
  end
end

function love.mousereleased(x, y, button)
  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    levelEditor.mousereleased(x, y, button)
  end
end

function love.mousemoved(x, y, dx, dy)
  if constants.DEV_MODE and levelEditor and levelEditor.isActive() then
    levelEditor.mousemoved(x, y, dx, dy)
    return
  end

  -- Handle mouse hover in menu
  if gameState.currentState == "menu" then
    ui.handleMenuHover(gameState, x, y)
  end
end
