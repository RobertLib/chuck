local colors = require("colors")

local levelobjects = {}

function levelobjects.drawPlatforms(gameState)
  -- Draw platforms
  for _, platform in ipairs(gameState.platforms) do
    -- Main platform body
    love.graphics.setColor(colors.platform)
    love.graphics.rectangle("fill", platform.x, platform.y, platform.width, platform.height)

    -- Top surface (lighter color for 3D effect)
    love.graphics.setColor(0.9, 0.7, 0.5)
    love.graphics.rectangle("fill", platform.x, platform.y, platform.width, 3)

    -- Side edges for 3D effect
    love.graphics.setColor(0.6, 0.4, 0.2)                                                                    -- Darker edge
    love.graphics.rectangle("fill", platform.x, platform.y + 3, 2, platform.height - 3)                      -- Left edge
    love.graphics.rectangle("fill", platform.x + platform.width - 2, platform.y + 3, 2, platform.height - 3) -- Right edge

    -- Bottom edge
    love.graphics.rectangle("fill", platform.x, platform.y + platform.height - 2, platform.width, 2)

    -- Add some texture/detail lines for wooden plank effect
    love.graphics.setColor(0.7, 0.5, 0.3)
    for i = 0, platform.width - 8, 16 do
      if platform.x + i + 1 < platform.x + platform.width - 1 then
        love.graphics.rectangle("fill", platform.x + i + 8, platform.y + 1, 1, platform.height - 2) -- Vertical wood grain
      end
    end

    -- Add small rivets or nails for detail
    love.graphics.setColor(0.4, 0.3, 0.2)
    for i = 8, platform.width - 8, 24 do
      if platform.x + i < platform.x + platform.width - 2 then
        love.graphics.rectangle("fill", platform.x + i, platform.y + 2, 2, 2)                     -- Top rivet
        if platform.height > 8 then
          love.graphics.rectangle("fill", platform.x + i, platform.y + platform.height - 4, 2, 2) -- Bottom rivet
        end
      end
    end
  end
end

function levelobjects.drawLadders(gameState)
  -- Draw ladders
  love.graphics.setColor(colors.ladder)
  for _, ladder in ipairs(gameState.ladders) do
    -- Draw side rails (vertical supports)
    local railWidth = 4
    local railSpacing = ladder.width - railWidth

    -- Left rail
    love.graphics.rectangle("fill", ladder.x, ladder.y, railWidth, ladder.height)
    -- Right rail
    love.graphics.rectangle("fill", ladder.x + railSpacing, ladder.y, railWidth, ladder.height)

    -- Draw ladder rungs (horizontal steps)
    love.graphics.setColor(0.7, 0.5, 0.3) -- Slightly different color for rungs
    local rungSpacing = 12
    local rungThickness = 3

    for i = 0, ladder.height, rungSpacing do
      if ladder.y + i + rungThickness <= ladder.y + ladder.height then
        love.graphics.rectangle("fill", ladder.x, ladder.y + i, ladder.width, rungThickness)
      end
    end

    -- Reset color for next objects
    love.graphics.setColor(colors.ladder)
  end
end

function levelobjects.drawSpikes(gameState)
  -- Draw spikes (dangerous traps)
  for _, spike in ipairs(gameState.spikes) do
    local x = spike.x
    local y = spike.y
    local width = spike.width
    local height = spike.height

    -- Main spike base (metallic gray)
    love.graphics.setColor(colors.spike)
    love.graphics.rectangle("fill", x, y + height - 4, width, 4) -- Base plate

    -- Draw individual spikes
    love.graphics.setColor(0.9, 0.9, 0.95)   -- Light metallic color for spike tips
    local spikeCount = math.floor(width / 6) -- Number of individual spikes
    local spikeWidth = width / spikeCount

    for i = 0, spikeCount - 1 do
      local spikeX = x + i * spikeWidth
      local spikeY = y

      -- Draw triangular spike using lines/polygons
      local vertices = {
        spikeX + spikeWidth / 2, spikeY,             -- Top point
        spikeX + 1, spikeY + height - 4,             -- Bottom left
        spikeX + spikeWidth - 1, spikeY + height - 4 -- Bottom right
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

return levelobjects
