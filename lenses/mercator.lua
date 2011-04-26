map = 0
maxy = 0

function inverse(x,y)

   if math.abs(y) > maxy then
      return 0
   end

   local lon = x
   local lat = math.atan(math.sinh(y))
   if math.abs(lon) > math.pi then
      return 0
   end

   local clat = math.cos(lat)

   return {
      math.sin(lon)*clat,
      math.sin(lat),
      math.cos(lon)*clat}
end

function init(fov,width,height,frame)

   if frame == width then
      if fov > math.pi*2 then
         return 0
      end
      scale = fov / frame;
   elseif frame == height then
      if fov > math.pi then
         return 0
      end
      scale = fov / frame;
   else
      return 0
   end

   maxy = math.log(math.tan(math.pi*0.5));
   return 1
end
