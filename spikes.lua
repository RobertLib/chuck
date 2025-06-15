local constants = require("constants")
local collision = require("collision")
local playerDeath = require("player_death")
local audio = require("audio")

local spikes = {}

-- Spike colors
local SPIKE_COLORS = {
  spike = { 0.7, 0.7, 0.8 }
}

-- Initialize a spike
function spikes.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width or constants.SPIKE_WIDTH,
    height = height or constants.SPIKE_HEIGHT
  }
end

-- Draw spikes (dangerous traps)
function spikes.draw(gameState)
  for _, spike in ipairs(gameState.spikes) do
    local x = spike.x
    local y = spike.y
    local width = spike.width
    local height = spike.height

    -- Main spike base (metallic gray)
    love.graphics.setColor(SPIKE_COLORS.spike)
    love.graphics.rectangle("fill", x, y + height, width, 4) -- Base plate

    -- Draw individual spikes
    love.graphics.setColor(0.9, 0.9, 0.95)   -- Light metallic color for spike tips
    local spikeCount = math.floor(width / 6) -- Number of individual spikes
    local spikeWidth = width / spikeCount

    for i = 0, spikeCount - 1 do
      local spikeX = x + i * spikeWidth
      local spikeY = y

      -- Draw triangular spike using lines/polygons
      local vertices = {
        spikeX + spikeWidth / 2, spikeY,         -- Top point
        spikeX + 1, spikeY + height,             -- Bottom left
        spikeX + spikeWidth - 1, spikeY + height -- Bottom right
      }

      -- Main spike body
      love.graphics.polygon("fill", vertices)

      -- Darker edges for 3D effect
      love.graphics.setColor(0.5, 0.5, 0.6)
      love.graphics.polygon("line", vertices)

      -- Reset color for next spike
      love.graphics.setColor(0.9, 0.9, 0.95)
    end

    -- Add some metallic shine effect
    love.graphics.setColor(1, 1, 1, 0.5)                                 -- Semi-transparent white highlight
    love.graphics.rectangle("fill", x + 2, y + height - 3, width - 4, 1) -- Base highlight
  end
end

function spikes.checkCollision(gameState)
  local player = gameState.player

  -- Collision with spikes (deadly trap)
  for _, spike in ipairs(gameState.spikes) do
    if collision.checkCollision(player, spike) then
      audio.playSpikes() -- Play spike sound
      if playerDeath.killPlayerAtCenter(gameState, "spike") then
        return true      -- Only lose one life per frame
      end
    end
  end
  return false
end

return spikes
