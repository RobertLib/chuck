local constants = require("constants")
local colors = require("colors")

local rendering = {}

-- Function to draw top status bar
function rendering.drawTopStatusBar(gameState)
  -- Draw background bar (matching game background but lighter)
  love.graphics.setColor(0.12, 0.12, 0.25, 0.85) -- Darker blue-purple matching background
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT)

  -- Draw subtle gradient effect
  love.graphics.setColor(0.16, 0.16, 0.32, 0.35) -- Slightly lighter for gradient
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT / 2)

  -- Draw thin border at bottom with platform color
  love.graphics.setColor(colors.platform[1], colors.platform[2], colors.platform[3], 0.6)
  love.graphics.rectangle("fill", 0, constants.STATUS_BAR_HEIGHT - 2, constants.SCREEN_WIDTH, 2)

  -- Left side - Score and Level
  love.graphics.setColor(1, 0.9, 0.2) -- Brighter gold for score
  love.graphics.print("Score: " .. gameState.score, constants.STATUS_BAR_PADDING, 10)

  love.graphics.setColor(0.6, 0.9, 1) -- Brighter light blue for level
  love.graphics.print("Level: " .. gameState.level, constants.STATUS_BAR_PADDING + 120, 10)

  -- Center - Time remaining
  local timeText = "Time: " .. math.ceil(math.max(0, gameState.timeLeft))
  local font = love.graphics.getFont()
  local timeWidth = font:getWidth(timeText)
  local timeX = (constants.SCREEN_WIDTH - timeWidth) / 2

  -- Color time based on remaining time
  if gameState.timeLeft <= 10 then
    love.graphics.setColor(1, 0.2, 0.2) -- Red when time is low
  elseif gameState.timeLeft <= 20 then
    love.graphics.setColor(1, 0.8, 0.2) -- Orange when time is medium
  else
    love.graphics.setColor(0.8, 1, 0.8) -- Light green when time is good
  end
  love.graphics.print(timeText, timeX, 10)

  -- Right side - Lives
  love.graphics.setColor(0.4, 1, 0.6) -- Green for lives (positive, healthy)
  local livesText = "Lives: " .. gameState.lives .. "/3"

  -- Calculate position to align right
  local textWidth = font:getWidth(livesText)
  love.graphics.print(livesText, constants.SCREEN_WIDTH - textWidth - constants.STATUS_BAR_PADDING, 10)

  -- Add invulnerability indicator if active (below the main status bar)
  if gameState.invulnerable then
    love.graphics.setColor(1, 1, 0, 0.7 + 0.3 * math.sin(gameState.globalTime * 8)) -- Flashing yellow
    local invulText = "* INVULNERABLE *"
    local invulWidth = font:getWidth(invulText)
    love.graphics.print(invulText, (constants.SCREEN_WIDTH - invulWidth) / 2, constants.STATUS_BAR_HEIGHT + 5)
  end
end

function rendering.drawPlatforms(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawPlatforms(gameState)
end

function rendering.drawCrumblingPlatforms(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawCrumblingPlatforms(gameState)
end

function rendering.drawLadders(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawLadders(gameState)
end

function rendering.drawCrates(gameState)
  local crates = require("crates")
  crates.drawCrates(gameState)
end

function rendering.drawSpikes(gameState)
  local levelobjects = require("levelobjects")
  levelobjects.drawSpikes(gameState)
end

function rendering.drawCollectibles(gameState)
  local collectibles = require("collectibles")
  -- Draw collectible items (gems/crystals)
  for _, collectible in ipairs(gameState.collectibles) do
    if not collectible.collected then
      collectibles.drawCollectibleItem(collectible)
    elseif collectible.isPickingUp then
      -- Draw pickup animation for collected items
      collectibles.drawPickupAnimation(collectible)
    end
  end
end

function rendering.drawEnemies(gameState)
  local enemies = require("enemies")
  -- Draw enemies
  for _, enemy in ipairs(gameState.enemies) do
    enemies.drawEnemy(enemy)
  end
end

function rendering.drawBats(gameState)
  local bats = require("bats")
  bats.drawBats(gameState)
end

function rendering.drawDecorations(gameState)
  local decorations = require("decorations")
  decorations.drawDecorations(gameState)
end

function rendering.drawBackground(gameState)
  -- Draw animated background using shader
  if gameState.backgroundShader then
    love.graphics.setShader(gameState.backgroundShader)
    gameState.backgroundShader:send("time", gameState.globalTime)
    gameState.backgroundShader:send("resolution", { constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT })

    -- Draw a full-screen rectangle to apply the shader
    love.graphics.setColor(1, 1, 1, 1)
    love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT)

    -- Reset shader
    love.graphics.setShader()
  else
    -- Fallback if shader fails
    love.graphics.setBackgroundColor(colors.background)
  end
end

function rendering.drawGameMessages(gameState, levelCount)
  -- Game state messages with exact status bar style
  if gameState.gameOver then
    -- Game Over message
    local font = love.graphics.getFont()
    local message = "GAME OVER!"
    local subtitle = "Press R to restart"

    local messageWidth = font:getWidth(message)
    local subtitleWidth = font:getWidth(subtitle)
    local maxWidth = math.max(messageWidth, subtitleWidth)
    local boxWidth = maxWidth + 60
    local boxHeight = 80
    local boxX = (constants.SCREEN_WIDTH - boxWidth) / 2
    local boxY = (constants.SCREEN_HEIGHT - boxHeight) / 2

    -- Main background (exact same as status bar)
    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    -- Subtle gradient effect (exact same as status bar)
    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    -- Border around panel (same color as status bar border)
    love.graphics.setColor(colors.platform[1], colors.platform[2], colors.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

    -- Main message text
    love.graphics.setColor(1, 0.7, 0.7) -- Light red
    love.graphics.printf(message, boxX, boxY + 15, boxWidth, "center")

    -- Subtitle text
    love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
    love.graphics.printf(subtitle, boxX, boxY + 45, boxWidth, "center")
  elseif gameState.showingLevelComplete then
    -- Level Complete message
    local font = love.graphics.getFont()
    local message = "LEVEL COMPLETE!"
    local subtitle = "Moving to next level..."

    local messageWidth = font:getWidth(message)
    local subtitleWidth = font:getWidth(subtitle)
    local maxWidth = math.max(messageWidth, subtitleWidth)
    local boxWidth = maxWidth + 60
    local boxHeight = 80
    local boxX = (constants.SCREEN_WIDTH - boxWidth) / 2
    local boxY = (constants.SCREEN_HEIGHT - boxHeight) / 2

    -- Main background (exact same as status bar)
    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    -- Subtle gradient effect (exact same as status bar)
    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    -- Border around panel (same color as status bar border)
    love.graphics.setColor(colors.platform[1], colors.platform[2], colors.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

    -- Main message text
    love.graphics.setColor(0.7, 1, 0.7) -- Light green
    love.graphics.printf(message, boxX, boxY + 15, boxWidth, "center")

    -- Subtitle text
    love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
    love.graphics.printf(subtitle, boxX, boxY + 45, boxWidth, "center")
  elseif gameState.won then
    if gameState.level >= levelCount then
      -- Victory message
      local font = love.graphics.getFont()
      local message = "CONGRATULATIONS!"
      local subtitle1 = "YOU WON THE GAME!"
      local subtitle2 = "Press R to restart"

      local messageWidth = font:getWidth(message)
      local subtitle1Width = font:getWidth(subtitle1)
      local subtitle2Width = font:getWidth(subtitle2)
      local maxWidth = math.max(messageWidth, subtitle1Width, subtitle2Width)
      local boxWidth = maxWidth + 60
      local boxHeight = 120
      local boxX = (constants.SCREEN_WIDTH - boxWidth) / 2
      local boxY = (constants.SCREEN_HEIGHT - boxHeight) / 2

      -- Main background (exact same as status bar)
      love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

      -- Subtle gradient effect (exact same as status bar)
      love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

      -- Border around panel (same color as status bar border)
      love.graphics.setColor(colors.platform[1], colors.platform[2], colors.platform[3], 0.6)
      love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
      love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
      love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

      -- Main message text
      love.graphics.setColor(1, 1, 0.7) -- Light golden
      love.graphics.printf(message, boxX, boxY + 15, boxWidth, "center")

      -- First subtitle text
      love.graphics.setColor(1, 0.9, 0.6) -- Golden
      love.graphics.printf(subtitle1, boxX, boxY + 45, boxWidth, "center")

      -- Second subtitle text
      love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
      love.graphics.printf(subtitle2, boxX, boxY + 85, boxWidth, "center")
    end
  end
end

return rendering
