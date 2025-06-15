local ladders = {}

-- Ladder colors
local LADDER_COLORS = {
  ladder = { 0.6, 0.4, 0.2 }
}

-- Initialize a ladder
function ladders.init(x, y, width, height)
  return {
    x = x,
    y = y,
    width = width,
    height = height
  }
end

-- Draw ladders
function ladders.draw(gameState)
  -- Draw ladders
  love.graphics.setColor(LADDER_COLORS.ladder)
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
    love.graphics.setColor(LADDER_COLORS.ladder)
  end
end

return ladders
