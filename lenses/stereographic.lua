map = "r_to_theta"

function r_to_theta(r)
   -- r = 2f*tan(theta/2)

   local el = 2*atan(r,2)
   if el > pi then
      return nil
   end
   return el
end

function init(fov,width,height,frame)
   return 2*tan(fov*0.25) / (frame*0.5)
end
