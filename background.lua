local constants = require("constants")

local background = {}

-- Background colors
local BACKGROUND_COLORS = {
  background = { 0.1, 0.1, 0.2 }
}

function background.draw(gameState)
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
    love.graphics.setBackgroundColor(BACKGROUND_COLORS.background)
  end
end

return background
