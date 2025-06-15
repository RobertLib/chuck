local collision = {}

function collision.checkCollision(rect1, rect2)
  return rect1.x < rect2.x + rect2.width and
      rect1.x + rect1.width > rect2.x and
      rect1.y < rect2.y + rect2.height and
      rect1.y + rect1.height > rect2.y
end

-- Check circular collision between a rectangle and a circle
function collision.checkCircleRectCollision(rect, circle)
  local rectCenterX = rect.x + rect.width / 2
  local rectCenterY = rect.y + rect.height / 2
  local circleCenterX = circle.x + circle.width / 2
  local circleCenterY = circle.y + circle.height / 2
  local radius = circle.width / 2

  -- Calculate distance between centers
  local dx = rectCenterX - circleCenterX
  local dy = rectCenterY - circleCenterY
  local distance = math.sqrt(dx * dx + dy * dy)

  -- Check if the distance is less than the sum of half the rectangle's diagonal and the circle's radius
  local rectHalfDiagonal = math.sqrt((rect.width / 2) ^ 2 + (rect.height / 2) ^ 2)

  return distance < (radius + rectHalfDiagonal)
end

return collision
