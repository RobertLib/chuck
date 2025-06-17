local colors = require("colors")
local crumbling_platforms = require("crumbling_platforms")

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

function levelobjects.drawCrumblingPlatforms(gameState)
  -- Draw crumbling platforms
  for _, platform in ipairs(gameState.crumbling_platforms) do
    local states = crumbling_platforms.getStates()

    if platform.state ~= states.DESTROYED then
      -- Calculate shake effect
      local shakeX = 0
      local shakeY = 0
      if platform.state == states.SHAKING then
        shakeX = platform.shakeOffset * (math.random() - 0.5)
        shakeY = platform.shakeOffset * 0.3 * (math.random() - 0.5)
      end

      -- Calculate opacity based on state
      local alpha = 1.0
      if platform.state == states.CRUMBLING then
        alpha = 1.0 - (platform.timer / 0.5) -- Fade out during crumbling
      end

      -- Main platform body (slightly different color to distinguish from normal platforms)
      love.graphics.setColor(0.8, 0.6, 0.4, alpha) -- Slightly more brown/weathered
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, platform.height)

      -- Top surface (lighter color for 3D effect)
      love.graphics.setColor(0.85, 0.65, 0.45, alpha)
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, 3)

      -- Side edges for 3D effect (darker)
      love.graphics.setColor(0.5, 0.3, 0.1, alpha)
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY + 3, 2, platform.height - 3)                      -- Left edge
      love.graphics.rectangle("fill", platform.x + platform.width - 2 + shakeX, platform.y + shakeY + 3, 2,
        platform.height - 3)                                                                                                     -- Right edge

      -- Bottom edge
      love.graphics.rectangle("fill", platform.x + shakeX, platform.y + platform.height - 2 + shakeY, platform.width, 2)

      -- Add cracks texture if shaking or crumbling
      if platform.state == states.SHAKING or platform.state == states.CRUMBLING then
        love.graphics.setColor(0.3, 0.2, 0.1, alpha)
        -- Draw crack lines
        for i = 0, platform.width, 8 do
          if math.random() > 0.7 then -- Random cracks
            love.graphics.rectangle("fill", platform.x + i + shakeX, platform.y + 2 + shakeY, 1, platform.height - 4)
          end
        end

        -- Draw horizontal cracks
        if platform.width > 16 then
          love.graphics.rectangle("fill", platform.x + 4 + shakeX, platform.y + platform.height / 2 + shakeY,
            platform.width - 8, 1)
        end
      else
        -- Normal wood texture for stable platforms
        love.graphics.setColor(0.6, 0.4, 0.2, alpha)
        for i = 0, platform.width - 8, 16 do
          if platform.x + i + 8 < platform.x + platform.width - 1 then
            love.graphics.rectangle("fill", platform.x + i + 8 + shakeX, platform.y + 1 + shakeY, 1, platform.height - 2)
          end
        end
      end

      -- Add warning color effect when shaking
      if platform.state == states.SHAKING then
        local warningAlpha = math.sin(platform.timer * 15) * 0.3 + 0.3
        love.graphics.setColor(1, 0.3, 0.3, warningAlpha * alpha) -- Red warning tint
        love.graphics.rectangle("fill", platform.x + shakeX, platform.y + shakeY, platform.width, platform.height)
      end
    end

    -- Draw particles
    for _, particle in ipairs(platform.particles) do
      local particleAlpha = particle.life / particle.maxLife
      love.graphics.setColor(0.7, 0.5, 0.3, particleAlpha)
      love.graphics.rectangle("fill", particle.x - particle.size / 2, particle.y - particle.size / 2, particle.size,
        particle.size)
    end
  end
end

return levelobjects
