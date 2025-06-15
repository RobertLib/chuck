local constants = require("constants")

local ui = {}

-- UI colors
local UI_COLORS = {
  platform = { 0.8, 0.6, 0.4 }
}

-- Function to draw a small heart for UI
local function drawUIHeart(x, y, scale, alpha)
  local size = 6 * scale -- Smaller than game hearts

  -- Main heart shape using circles and triangle
  love.graphics.setColor(1, 0.2, 0.2, alpha) -- Deep red

  -- Left circle of heart
  love.graphics.circle("fill", x - size * 0.3, y - size * 0.2, size * 0.4)
  -- Right circle of heart
  love.graphics.circle("fill", x + size * 0.3, y - size * 0.2, size * 0.4)
  -- Bottom triangle of heart
  love.graphics.polygon("fill",
    x - size * 0.6, y,
    x + size * 0.6, y,
    x, y + size * 0.7)

  -- Inner highlight
  love.graphics.setColor(1, 0.6, 0.6, alpha * 0.8)
  love.graphics.circle("fill", x - size * 0.2, y - size * 0.3, size * 0.15)
end

-- Function to draw top status bar
function ui.drawTopStatusBar(gameState)
  -- Draw background bar (matching game background but lighter)
  love.graphics.setColor(0.12, 0.12, 0.25, 0.2) -- Darker blue-purple matching background
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT)

  -- Draw subtle gradient effect
  love.graphics.setColor(0.16, 0.16, 0.32, 0.35)                                                 -- Slightly lighter for gradient
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.STATUS_BAR_HEIGHT / 2) -- Left side - Score and Level
  love.graphics.setColor(1, 1, 1)                                                                -- Reset color for text
  love.graphics.print("Score: " .. gameState.score, constants.STATUS_BAR_PADDING, 10)

  love.graphics.setColor(0.6, 0.9, 1) -- Brighter light blue for level
  love.graphics.print("Level: " .. gameState.level, constants.STATUS_BAR_PADDING + 150, 10)

  -- Left-center area - Items progress text
  local font = love.graphics.getFont()

  -- Items progress
  local collectedCount = 0
  local totalCount = #gameState.collectibles
  for _, collectible in ipairs(gameState.collectibles) do
    if collectible.collected then
      collectedCount = collectedCount + 1
    end
  end

  local itemsText = "Items: " .. collectedCount .. "/" .. totalCount

  -- Calculate positions for even distribution across status bar
  local leftSectionEnd = constants.STATUS_BAR_PADDING + 240 -- End of Level text
  local rightSectionStart = constants.SCREEN_WIDTH - 120    -- Start of hearts area
  local availableSpace = rightSectionStart - leftSectionEnd

  -- Position items text in the first part of available space
  local itemsX = leftSectionEnd + availableSpace * 0.1
  love.graphics.setColor(1, 0.9, 0.2) -- Gold color for items text
  love.graphics.print(itemsText, math.floor(itemsX), 10)

  -- Center area - Progress bar for time remaining in the middle
  local progressBarWidth = math.min(200, availableSpace * 0.4) -- Adaptive width
  local progressBarHeight = 8
  local progressBarX = leftSectionEnd + availableSpace * 0.725 - progressBarWidth * 0.5
  local progressBarY = 13

  -- Time progress calculation (how much time is left)
  local timeProgress = gameState.levelTimeLimit > 0 and (gameState.timeLeft / gameState.levelTimeLimit) or 1

  -- Draw progress bar background
  love.graphics.setColor(0.2, 0.2, 0.3, 0.8) -- Dark background
  love.graphics.rectangle("fill", progressBarX, progressBarY, progressBarWidth, progressBarHeight)

  -- Draw progress bar border
  love.graphics.setColor(0.5, 0.5, 0.6, 0.9) -- Light border
  love.graphics.rectangle("line", progressBarX, progressBarY, progressBarWidth, progressBarHeight)

  -- Draw time progress fill
  if timeProgress > 0 then
    local fillWidth = progressBarWidth * timeProgress

    -- Color based on remaining time
    if timeProgress <= 0.2 then
      love.graphics.setColor(1, 0.2, 0.2, 0.9) -- Red when time is low
    elseif timeProgress <= 0.4 then
      love.graphics.setColor(1, 0.8, 0.2, 0.9) -- Orange when time is medium
    elseif timeProgress <= 0.7 then
      love.graphics.setColor(1, 1, 0.3, 0.9)   -- Yellow when time is getting low
    else
      love.graphics.setColor(0.2, 1, 0.2, 0.9) -- Green when plenty of time
    end

    love.graphics.rectangle("fill", progressBarX, progressBarY, fillWidth, progressBarHeight)
  end

  -- Right side - Lives as hearts
  local heartSize = 1.0   -- Scale for UI hearts
  local heartSpacing = 16 -- Space between hearts
  local heartY = 17       -- Y position for hearts

  -- Limit display to maximum 5 hearts
  local heartsToShow = math.min(gameState.lives, 5)
  local showPlusSign = gameState.lives > 5

  -- Calculate total width needed (hearts + plus sign if needed)
  local totalHeartsWidth = (heartsToShow - 1) * heartSpacing
  local plusSignWidth = showPlusSign and 20 or 0 -- Space for "+" sign
  local totalWidth = totalHeartsWidth + plusSignWidth
  local startX = constants.SCREEN_WIDTH - totalWidth - constants.STATUS_BAR_PADDING - 8

  -- Draw hearts (maximum 5)
  for i = 1, heartsToShow do
    local heartX = startX + (i - 1) * heartSpacing
    drawUIHeart(heartX, heartY, heartSize, 1.0)
  end

  -- Draw plus sign if there are more than 5 lives
  if showPlusSign then
    love.graphics.setColor(1, 0.9, 0.2) -- Gold color for plus sign
    local plusX = startX + heartsToShow * heartSpacing
    love.graphics.print("+", plusX, heartY - 8)
  end

  -- Add invulnerability indicator if active (below the main status bar)
  if gameState.invulnerable then
    love.graphics.setColor(1, 1, 0, 0.7 + 0.3 * math.sin(gameState.globalTime * 8)) -- Flashing yellow
    local invulText = "* INVULNERABLE *"
    local invulWidth = font:getWidth(invulText)
    love.graphics.print(invulText, math.floor((constants.SCREEN_WIDTH - invulWidth) / 2),
      math.floor(constants.STATUS_BAR_HEIGHT + 5))
  end
end

function ui.drawGameMessages(gameState, levelCount, levels)
  -- Show error message if level loading failed
  if levels.loadError then
    local font = love.graphics.getFont()
    local message = "ERROR: " .. tostring(levels.loadError)
    local subtitle = "Check your game files and restart."
    local messageWidth = font:getWidth(message)
    local subtitleWidth = font:getWidth(subtitle)
    local maxWidth = math.max(messageWidth, subtitleWidth)
    local boxWidth = maxWidth + 60
    local boxHeight = 100
    local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
    local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

    love.graphics.setColor(0.2, 0.05, 0.05, 0.95)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)
    love.graphics.setColor(1, 0.3, 0.3)
    love.graphics.printf(message, boxX, math.floor(boxY + 20), boxWidth, "center")
    love.graphics.setColor(1, 1, 1)
    love.graphics.printf(subtitle, boxX, math.floor(boxY + 60), boxWidth, "center")
    return
  end

  -- Game state messages with exact status bar style
  if gameState.gameOver then
    -- Game Over message
    local font = love.graphics.getFont()
    local message = "GAME OVER!"
    local subtitle = "Press R to return to menu"

    local messageWidth = font:getWidth(message)
    local subtitleWidth = font:getWidth(subtitle)
    local maxWidth = math.max(messageWidth, subtitleWidth)
    local boxWidth = maxWidth + 60
    local boxHeight = 80
    local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
    local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

    -- Main background (exact same as status bar)
    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    -- Subtle gradient effect (exact same as status bar)
    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    -- Border around panel (same color as status bar border)
    love.graphics.setColor(UI_COLORS.platform[1], UI_COLORS.platform[2], UI_COLORS.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

    -- Main message text
    love.graphics.setColor(1, 0.7, 0.7) -- Light red
    love.graphics.printf(message, boxX, math.floor(boxY + 15), boxWidth, "center")

    -- Subtitle text
    love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
    love.graphics.printf(subtitle, boxX, math.floor(boxY + 45), boxWidth, "center")
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
    local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
    local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

    -- Main background (exact same as status bar)
    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    -- Subtle gradient effect (exact same as status bar)
    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    -- Border around panel (same color as status bar border)
    love.graphics.setColor(UI_COLORS.platform[1], UI_COLORS.platform[2], UI_COLORS.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

    -- Main message text
    love.graphics.setColor(0.7, 1, 0.7) -- Light green
    love.graphics.printf(message, boxX, math.floor(boxY + 15), boxWidth, "center")

    -- Subtitle text
    love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
    love.graphics.printf(subtitle, boxX, math.floor(boxY + 45), boxWidth, "center")
  elseif gameState.won then
    if gameState.level >= levelCount then
      -- Victory message
      local font = love.graphics.getFont()
      local message = "CONGRATULATIONS!"
      local subtitle1 = "YOU WON THE GAME!"
      local subtitle2 = "Press R to return to menu"

      local messageWidth = font:getWidth(message)
      local subtitle1Width = font:getWidth(subtitle1)
      local subtitle2Width = font:getWidth(subtitle2)
      local maxWidth = math.max(messageWidth, subtitle1Width, subtitle2Width)
      local boxWidth = maxWidth + 60
      local boxHeight = 120
      local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
      local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

      -- Main background (exact same as status bar)
      love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

      -- Subtle gradient effect (exact same as status bar)
      love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

      -- Border around panel (same color as status bar border)
      love.graphics.setColor(UI_COLORS.platform[1], UI_COLORS.platform[2], UI_COLORS.platform[3], 0.6)
      love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
      love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
      love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
      love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

      -- Main message text
      love.graphics.setColor(1, 1, 0.7) -- Light golden
      love.graphics.printf(message, boxX, math.floor(boxY + 15), boxWidth, "center")

      -- First subtitle text
      love.graphics.setColor(1, 0.9, 0.6) -- Golden
      love.graphics.printf(subtitle1, boxX, math.floor(boxY + 45), boxWidth, "center")

      -- Second subtitle text
      love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
      love.graphics.printf(subtitle2, boxX, math.floor(boxY + 85), boxWidth, "center")
    end
  elseif gameState.paused then
    -- Pause message
    local font = love.graphics.getFont()
    local message = "PAUSED"
    local subtitle1 = "Press P to resume"
    local subtitle2 = "Press ESC to return to menu"

    local messageWidth = font:getWidth(message)
    local subtitle1Width = font:getWidth(subtitle1)
    local subtitle2Width = font:getWidth(subtitle2)
    local maxWidth = math.max(messageWidth, subtitle1Width, subtitle2Width)
    local boxWidth = maxWidth + 60
    local boxHeight = 120
    local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
    local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

    -- Main background (exact same as status bar)
    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    -- Subtle gradient effect (exact same as status bar)
    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    -- Border around panel (same color as status bar border)
    love.graphics.setColor(UI_COLORS.platform[1], UI_COLORS.platform[2], UI_COLORS.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2) -- Bottom border
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)                 -- Top border
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)                -- Left border
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight) -- Right border

    -- Main message text
    love.graphics.setColor(0.7, 0.7, 1) -- Light blue
    love.graphics.printf(message, boxX, math.floor(boxY + 15), boxWidth, "center")

    -- Subtitle text
    love.graphics.setColor(0.9, 0.9, 0.9) -- Light gray
    love.graphics.printf(subtitle1, boxX, math.floor(boxY + 45), boxWidth, "center")

    -- Second subtitle text
    love.graphics.setColor(0.8, 0.8, 0.8) -- Slightly darker gray
    love.graphics.printf(subtitle2, boxX, math.floor(boxY + 75), boxWidth, "center")
  elseif gameState.showingTimesUp then
    -- Times Up message
    local font = love.graphics.getFont()
    local message = "TIME'S UP!"
    local subtitle = "Restarting level..."

    local messageWidth = font:getWidth(message)
    local subtitleWidth = font:getWidth(subtitle)
    local maxWidth = math.max(messageWidth, subtitleWidth)
    local boxWidth = maxWidth + 60
    local boxHeight = 80
    local boxX = math.floor((constants.SCREEN_WIDTH - boxWidth) / 2)
    local boxY = math.floor((constants.SCREEN_HEIGHT - boxHeight) / 2)

    love.graphics.setColor(0.12, 0.12, 0.25, 0.85)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight)

    love.graphics.setColor(0.16, 0.16, 0.32, 0.35)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, boxHeight / 2)

    love.graphics.setColor(UI_COLORS.platform[1], UI_COLORS.platform[2], UI_COLORS.platform[3], 0.6)
    love.graphics.rectangle("fill", boxX, boxY + boxHeight - 2, boxWidth, 2)
    love.graphics.rectangle("fill", boxX, boxY, boxWidth, 2)
    love.graphics.rectangle("fill", boxX, boxY, 2, boxHeight)
    love.graphics.rectangle("fill", boxX + boxWidth - 2, boxY, 2, boxHeight)

    love.graphics.setColor(1, 0.9, 0.4) -- Yellow
    love.graphics.printf(message, boxX, math.floor(boxY + 15), boxWidth, "center")

    love.graphics.setColor(0.9, 0.9, 0.9)
    love.graphics.printf(subtitle, boxX, math.floor(boxY + 45), boxWidth, "center")
  end
end

-- Function to draw main menu
function ui.drawMainMenu(gameState)
  -- Draw background overlay
  love.graphics.setColor(0.05, 0.05, 0.15, 0.95)
  love.graphics.rectangle("fill", 0, 0, constants.SCREEN_WIDTH, constants.SCREEN_HEIGHT)

  -- Draw title with shadow effect
  love.graphics.setColor(0.1, 0.1, 0.1, 0.8) -- Shadow
  local titleFont = love.graphics.getFont()
  local title = "CHUCK"
  local titleWidth = titleFont:getWidth(title)
  local titleX = math.floor((constants.SCREEN_WIDTH - titleWidth) / 2)
  local titleY = math.floor(constants.SCREEN_HEIGHT / 4)
  love.graphics.print(title, math.floor(titleX + 3), math.floor(titleY + 3))

  love.graphics.setColor(1, 0.9, 0.2) -- Gold color for title
  love.graphics.print(title, titleX, titleY)

  -- Draw subtitle
  love.graphics.setColor(0.8, 0.8, 0.9) -- Light purple
  local subtitle = "A Platform Adventure Game"
  local subtitleWidth = titleFont:getWidth(subtitle)
  local subtitleX = math.floor((constants.SCREEN_WIDTH - subtitleWidth) / 2)
  love.graphics.print(subtitle, subtitleX, math.floor(titleY + 40))

  -- Draw menu options with background boxes
  -- Calculate menu positioning dynamically based on number of options
  local optionSpacing = 50 -- Reduced spacing
  local totalMenuHeight = (#gameState.menuOptions - 1) * optionSpacing
  local instructionsY = constants.SCREEN_HEIGHT - 150
  local availableSpace = instructionsY - (titleY + 80) -- Space between subtitle and instructions
  local optionY = math.floor(titleY + 80 + (availableSpace - totalMenuHeight) / 2)

  for i, option in ipairs(gameState.menuOptions) do
    local y = math.floor(optionY + (i - 1) * optionSpacing)
    local optionWidth = titleFont:getWidth(option)
    local optionX = math.floor((constants.SCREEN_WIDTH - optionWidth) / 2)

    if i == gameState.menuSelectedOption then
      -- Selected option background with subtle pulsing effect
      local pulseIntensity = 0.1 + 0.05 * math.sin(gameState.globalTime * 3)
      love.graphics.setColor(0.2 + pulseIntensity, 0.3 + pulseIntensity, 0.5 + pulseIntensity, 0.7)
      love.graphics.rectangle("fill", optionX - 20, y - 5, optionWidth + 40, 30)

      -- Selected option border with glow effect
      love.graphics.setColor(1, 0.9, 0.2, 0.8 + 0.2 * math.sin(gameState.globalTime * 4))
      love.graphics.rectangle("line", optionX - 20, y - 5, optionWidth + 40, 30)

      -- Selected option text
      love.graphics.setColor(1, 1, 1) -- White
      love.graphics.print(option, optionX, y + 2)
    else
      -- Non-selected option with subtle hover background
      love.graphics.setColor(0.1, 0.1, 0.2, 0.3)
      love.graphics.rectangle("fill", optionX - 15, y - 2, optionWidth + 30, 24)

      -- Non-selected option text
      love.graphics.setColor(0.7, 0.7, 0.7) -- Gray
      love.graphics.print(option, optionX, y + 2)
    end
  end

  -- Draw instructions
  love.graphics.setColor(0.6, 0.6, 0.6) -- Light gray
  local instructionsY = constants.SCREEN_HEIGHT - 100
  local instructions1 = "Use UP/DOWN arrows or MOUSE to navigate, ENTER or CLICK to select, ESC to quit"
  local instructions2 = "Press F11 for quick fullscreen toggle"
  local instructionsWidth1 = titleFont:getWidth(instructions1)
  local instructionsWidth2 = titleFont:getWidth(instructions2)
  local instructionsX1 = math.floor((constants.SCREEN_WIDTH - instructionsWidth1) / 2)
  local instructionsX2 = math.floor((constants.SCREEN_WIDTH - instructionsWidth2) / 2)
  love.graphics.print(instructions1, instructionsX1, instructionsY)
  love.graphics.print(instructions2, instructionsX2, math.floor(instructionsY + 20))

  -- Draw version info
  love.graphics.setColor(0.4, 0.4, 0.4) -- Dark gray
  local version = constants.VERSION
  local versionY = constants.SCREEN_HEIGHT - 30
  love.graphics.print(version, 10, versionY)
end

-- Function to check if mouse position is over a menu option
function ui.getMenuOptionAtPosition(gameState, mouseX, mouseY)
  local titleFont = love.graphics.getFont()
  local titleY = math.floor(constants.SCREEN_HEIGHT / 4)

  -- Calculate menu positioning (same as in drawMainMenu)
  local optionSpacing = 50
  local totalMenuHeight = (#gameState.menuOptions - 1) * optionSpacing
  local instructionsY = constants.SCREEN_HEIGHT - 150
  local availableSpace = instructionsY - (titleY + 80)
  local optionY = math.floor(titleY + 80 + (availableSpace - totalMenuHeight) / 2)

  for i, option in ipairs(gameState.menuOptions) do
    local y = math.floor(optionY + (i - 1) * optionSpacing)
    local optionWidth = titleFont:getWidth(option)
    local optionX = math.floor((constants.SCREEN_WIDTH - optionWidth) / 2)

    -- Check if mouse is within the menu option bounds (with padding)
    if mouseX >= optionX - 20 and mouseX <= optionX + optionWidth + 20 and
        mouseY >= y - 5 and mouseY <= y + 25 then
      return i
    end
  end

  return nil
end

-- Function to handle mouse hover in menu
function ui.handleMenuHover(gameState, mouseX, mouseY)
  local hoveredOption = ui.getMenuOptionAtPosition(gameState, mouseX, mouseY)
  if hoveredOption then
    gameState.menuSelectedOption = hoveredOption
  end
end

-- Function to handle mouse click in menu
function ui.handleMenuClick(gameState, mouseX, mouseY)
  local clickedOption = ui.getMenuOptionAtPosition(gameState, mouseX, mouseY)
  if clickedOption then
    gameState.menuSelectedOption = clickedOption
    return true -- Indicate that a menu option was clicked
  end
  return false
end

return ui
