// SPC serial connector wire guide. 
// Design by Marius Gheorghescu, Oct 2018
//
// Suggested print settings: flexible filament, 0.15mm layer height
//

// bottom to top
top_height = 5.3;
top_width = 6.4;

clip_height = 3.15;
clip_width = 7.75;

base_height = 1.25;
base_width = 9.25;

connector_depth = 8;

retention_clip_depth = 2.75;
retention_clip_radius = 0.4;

epsilon = 0.001;

pin_spacing = 1.6;

pin_depth = 0.5;
pin_width = 0.75;

pin_spacing_connector = 2.6;

module wire_connector()
{
    wire_dia_coated_x = 1.2;
    wire_dia_coated_z = 1.4;
    wire_dia = 0.7;
    wire_spacing_factor = 1.3;
    
    grip_depth = 10;
    
    difference() {

        $fn = 30;
        
        union() {
            
            // base
            translate([connector_depth/2,0,base_height/2])
                cube([connector_depth, base_width, base_height], center=true);

            // grip
            translate([-grip_depth/2 - 1,0, top_height/2])
                cube([grip_depth, base_width+1, top_height], center=true);
            
            translate([-grip_depth/2,0, top_height/2])
                cube([grip_depth, base_width, top_height], center=true);
            
            // mid-point block
            translate([connector_depth/2,0,clip_height/2])
                cube([connector_depth, clip_width, clip_height], center=true);

            // top block
            translate([connector_depth/2,0,top_height/2])
                cube([connector_depth, top_width, top_height], center=true);
            
            // retention clips
            color("red")
            translate([retention_clip_depth, clip_width/2, clip_height/2])
                cylinder(r=retention_clip_radius, h=clip_height, center=true);

            color("red")
            translate([retention_clip_depth, -clip_width/2, clip_height/2])
                cylinder(r=retention_clip_radius, h=clip_height, center=true);
        }
        
        // see-through cut-out
        // translate([-10,-10,0]) cube([20,20,20], center=true);

        // front chamfer 
        rotate([0,35,0])
        translate([connector_depth/2,0,top_height + top_height/2 + 1])
            cube([connector_depth, top_width+epsilon, top_height/4], center=true);
        

        // chamfer corners
        {
            translate([connector_depth, base_width, top_height/2 - epsilon])
            rotate([0,0,45])
            cube([connector_depth, base_width, top_height], center=true);

            translate([connector_depth, -base_width, top_height/2 - epsilon])
            rotate([0,0,-45])
            cube([connector_depth, base_width, top_height], center=true);
        }        

        // ribs
        for(i=[1:2:grip_depth]) {
            translate([-1-i,-base_width/2-1,top_height/2])
                rotate([0,0,-30])
                cylinder(r=1, h=top_height+epsilon, $fn=6, center=true);

            translate([-1-i,base_width/2+1,top_height/2])
                rotate([0,0,30])
                cylinder(r=1, h=top_height+epsilon, $fn=6, center=true);


            translate([-1-i,0,top_height+0.5])
            rotate([0,90,90])
                cylinder(r=1, h=base_width+2, $fn=6, center=true);
        }

        // pin contact area guide
        // translate([connector_depth - 2.45,0,0])%cube([2.40, 5.54, 1], center=true);
        
        for(i=[-1.5:1:1.5]) {

            // contact channels
            #translate([connector_depth/2, i*pin_spacing, 0])
                cube([connector_depth/2, pin_width, pin_depth], center=true);

            // top attach
            hull() {
            translate([connector_depth*0.75, i*pin_spacing, top_height - wire_dia_coated_z/2 + epsilon])
                cube([connector_depth/2, wire_dia, wire_dia_coated_z], center=true);

            translate([connector_depth*0.75, i*pin_spacing, top_height - wire_dia_coated_z/2 + epsilon])
                rotate([0,25,0])
                cube([connector_depth/2, 0.01, wire_dia_coated_z], center=true);
            }

            // slicer path width guide
            //translate([0, -top_width/2 + 0.2,0])
            //%color("blue", 0.2) cube([20, 0.4, top_height*1.5], center=true);

            // through wires
            translate([-connector_depth - epsilon, i*pin_spacing*wire_spacing_factor, top_height/2])
                cube([connector_depth, wire_dia_coated_x, wire_dia_coated_z], center=true);

            // front contact lift 
            translate([connector_depth - 1, i*pin_spacing, pin_depth/2 - epsilon])
                rotate([0,-35,0])
                cube([connector_depth, wire_dia, 2*pin_depth], center=true);

            // wire front channel
            hull() {
                translate([connector_depth - wire_dia/2 + epsilon, i*pin_spacing, 0])
                    cube([wire_dia, wire_dia, wire_dia], center=true);

                hull() {
                    translate([connector_depth - wire_dia/2 + epsilon, i*pin_spacing, top_height/2])
                        cube([wire_dia, wire_dia, top_height], center=true);
                    translate([connector_depth - wire_dia/2 + epsilon, i*pin_spacing, top_height/2])
                        cube([wire_dia*2, 0.01, top_height], center=true);
                }
            }

            // back
            #hull() {
                translate([connector_depth/2, i*pin_spacing, 0])
                    cube([2*wire_dia, wire_dia, wire_dia], center=true);

                translate([-connector_depth/2, i*pin_spacing*wire_spacing_factor, top_height/2])
                    cube([wire_dia_coated_x, wire_dia_coated_x, wire_dia_coated_z], center=true);
            }

        }
    }
}

color("green")
    wire_connector();