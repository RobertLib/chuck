local platforms = {}

-- Platform colors
local PLATFORM_COLORS = {
  platform = { 0.8, 0.6, 0.4 }
}

-- Initialize a platform
function platforms.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width,
    height = height
  }
end

-- Draw platforms
function platforms.draw(gameState)
  -- Draw platforms
  for _, platform in ipairs(gameState.platforms) do
    -- Shadow (bottom right)
    love.graphics.setColor(0, 0, 0, 0.1) -- Black shadow with transparency
    love.graphics.rectangle("fill", platform.x + 4, platform.y + 4, platform.width, platform.height)

    -- Main platform body
    love.graphics.setColor(PLATFORM_COLORS.platform)
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

return platforms
