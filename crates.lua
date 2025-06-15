local constants = require("constants")
local collision = require("collision")
local audio = require("audio")

local crates = {}

-- Crate colors
local CRATE_COLORS = {
  crate = { 0.6, 0.4, 0.2 }
}

-- Initialize a crate
function crates.init(x, y)
  return {
    x = x,
    y = y,
    width = constants.CRATE_SIZE,
    height = constants.CRATE_SIZE
  }
end

-- Function to check if a crate can be moved to a new position
function crates.canMoveCrate(crate, newX, newY, gameState)
  local newCrate = { x = newX, y = newY, width = crate.width, height = crate.height }

  -- Check screen boundaries
  if newX < 0 or newX + crate.width > constants.SCREEN_WIDTH then
    return false
  end

  -- Check collision with platforms
  for _, platform in ipairs(gameState.platforms) do
    if collision.checkCollision(newCrate, platform) then
      return false
    end
  end

  -- Check collision with other crates
  for _, otherCrate in ipairs(gameState.crates) do
    if otherCrate ~= crate and collision.checkCollision(newCrate, otherCrate) then
      return false
    end
  end

  return true
end

-- Function to find crates that are standing on top of another crate
function crates.getCratesOnTop(baseCrate, gameState)
  local cratesOnTop = {}

  for _, otherCrate in ipairs(gameState.crates) do
    if otherCrate ~= baseCrate then
      -- Check if otherCrate is standing on top of baseCrate
      -- A crate is "on top" if its bottom is touching the top of the base crate
      local otherBottom = otherCrate.y + otherCrate.height
      local baseTop = baseCrate.y

      -- Check if they overlap horizontally and the other crate is just above
      if math.abs(otherBottom - baseTop) <= constants.COLLISION_TOLERANCE and -- Small tolerance for floating point precision
          otherCrate.x < baseCrate.x + baseCrate.width and
          otherCrate.x + otherCrate.width > baseCrate.x then
        table.insert(cratesOnTop, otherCrate)
      end
    end
  end

  return cratesOnTop
end

-- Function to try pushing a crate and all crates on top of it
function crates.tryPushCrate(player, crate, direction, dt, gameState)
  local pushSpeed = constants.PLAYER_SPEED * constants.CRATE_PUSH_SPEED_FACTOR -- Crates are slower/heavier than player
  local newX = crate.x + direction * pushSpeed * dt

  -- Get all crates that are standing on this crate (recursively)
  local function getAllCratesInStack(baseCrate, visited)
    visited = visited or {}

    -- Prevent infinite recursion by checking if we've already visited this crate
    if visited[baseCrate] then
      return {}
    end

    local stack = { baseCrate }
    visited[baseCrate] = true

    local cratesOnTop = crates.getCratesOnTop(baseCrate, gameState)
    for _, topCrate in ipairs(cratesOnTop) do
      if not visited[topCrate] then
        local subStack = getAllCratesInStack(topCrate, visited)
        for _, subCrate in ipairs(subStack) do
          table.insert(stack, subCrate)
        end
      end
    end

    return stack
  end

  local crateStack = getAllCratesInStack(crate)

  -- Check if all crates in the stack can be moved
  for _, stackCrate in ipairs(crateStack) do
    local crateNewX = stackCrate.x + direction * pushSpeed * dt
    if not crates.canMoveCrate(stackCrate, crateNewX, stackCrate.y, gameState) then
      return 0 -- Can't move the stack
    end
  end

  -- Move all crates in the stack
  local actualMovement = newX - crate.x
  for _, stackCrate in ipairs(crateStack) do
    stackCrate.x = stackCrate.x + actualMovement
  end

  return actualMovement
end

-- Function to check if crate is supported (on platform or other crate)
function crates.isCrateSupported(crate, gameState)
  if not crate then
    return false
  end

  -- Check if crate is on a platform
  for _, platform in ipairs(gameState.platforms) do
    if math.abs(crate.y + crate.height - platform.y) <= constants.SUPPORT_TOLERANCE and
        crate.x + crate.width > platform.x and
        crate.x < platform.x + platform.width then
      return true
    end
  end

  -- Check if crate is on another crate
  for _, otherCrate in ipairs(gameState.crates) do
    if otherCrate ~= crate and
        math.abs(crate.y + crate.height - otherCrate.y) <= constants.SUPPORT_TOLERANCE and
        crate.x + crate.width > otherCrate.x and
        crate.x < otherCrate.x + otherCrate.width then
      return true
    end
  end

  -- Check if crate is on ground
  if crate.y + crate.height >= constants.SCREEN_HEIGHT then
    return true
  end

  return false
end

-- Function to update crates (apply gravity)
function crates.updateCrates(dt, gameState)
  for _, crate in ipairs(gameState.crates) do
    -- Only apply gravity if crate is not supported
    if not crates.isCrateSupported(crate, gameState) then
      -- Store old position
      local oldY = crate.y

      -- Apply slower gravity to crates (they're heavier and fall more slowly)
      crate.y = crate.y + constants.CRATE_GRAVITY * dt

      -- Check collision with platforms
      for _, platform in ipairs(gameState.platforms) do
        if collision.checkCollision(crate, platform) then
          -- If crate hits platform from above
          if oldY + crate.height <= platform.y then
            crate.y = platform.y - crate.height
            break
          end
        end
      end

      -- Check collision with other crates below
      for _, otherCrate in ipairs(gameState.crates) do
        if otherCrate ~= crate and collision.checkCollision(crate, otherCrate) then
          -- If this crate is falling onto another crate
          if oldY + crate.height <= otherCrate.y then
            crate.y = otherCrate.y - crate.height
            break
          end
        end
      end

      -- Prevent crates from falling through the ground
      if crate.y + crate.height > constants.SCREEN_HEIGHT then
        crate.y = constants.SCREEN_HEIGHT - crate.height
      end
    end
  end
end

function crates.drawCrates(gameState)
  -- Draw crates
  for _, crate in ipairs(gameState.crates) do
    -- Main crate body
    love.graphics.setColor(CRATE_COLORS.crate)
    love.graphics.rectangle("fill", crate.x, crate.y, crate.width, crate.height)

    -- 3D effect - lighter top and left edges
    love.graphics.setColor(0.7, 0.5, 0.3)                              -- Lighter wood color
    love.graphics.rectangle("fill", crate.x, crate.y, crate.width, 2)  -- Top edge
    love.graphics.rectangle("fill", crate.x, crate.y, 2, crate.height) -- Left edge

    -- 3D effect - darker bottom and right edges
    love.graphics.setColor(0.4, 0.2, 0.1)                                                -- Darker wood color
    love.graphics.rectangle("fill", crate.x, crate.y + crate.height - 2, crate.width, 2) -- Bottom edge
    love.graphics.rectangle("fill", crate.x + crate.width - 2, crate.y, 2, crate.height) -- Right edge

    -- Wood planks texture - vertical lines
    love.graphics.setColor(0.5, 0.3, 0.15)
    for i = 6, crate.width - 6, 8 do
      love.graphics.rectangle("fill", crate.x + i, crate.y + 2, 1, crate.height - 4)
    end

    -- Metal reinforcement bands
    love.graphics.setColor(0.3, 0.3, 0.3) -- Dark metal color
    -- Horizontal bands
    love.graphics.rectangle("fill", crate.x + 2, crate.y + 6, crate.width - 4, 2)
    love.graphics.rectangle("fill", crate.x + 2, crate.y + crate.height - 8, crate.width - 4, 2)

    -- Corner reinforcements
    love.graphics.setColor(0.2, 0.2, 0.2)                                                        -- Very dark metal
    love.graphics.rectangle("fill", crate.x + 2, crate.y + 2, 3, 3)                              -- Top-left corner
    love.graphics.rectangle("fill", crate.x + crate.width - 5, crate.y + 2, 3, 3)                -- Top-right corner
    love.graphics.rectangle("fill", crate.x + 2, crate.y + crate.height - 5, 3, 3)               -- Bottom-left corner
    love.graphics.rectangle("fill", crate.x + crate.width - 5, crate.y + crate.height - 5, 3, 3) -- Bottom-right corner
  end
end

return crates
