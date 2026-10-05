-- vertex_sniffer.lua
-- Wireshark Lua Post-Dissector for Vertex Golf BLE GATT Packets

vertex_proto = Proto("vertex_golf", "Vertex Golf Sensor Data")

-- Define the hypothesized fields based on the Iottive engineering metrics
local f_speed = ProtoField.uint16("vertex_golf.speed", "Speed (raw)", base.DEC)
local f_face  = ProtoField.int16("vertex_golf.face", "Face Angle", base.DEC)
local f_twist = ProtoField.int16("vertex_golf.twist", "Twist", base.DEC)
local f_lie   = ProtoField.int16("vertex_golf.lie", "Lie Angle", base.DEC)
local f_lean  = ProtoField.int16("vertex_golf.lean", "Shaft Lean", base.DEC)
local f_loft  = ProtoField.int16("vertex_golf.loft", "Loft Angle", base.DEC)
local f_raw   = ProtoField.bytes("vertex_golf.raw", "Raw Payload")

vertex_proto.fields = { f_speed, f_face, f_twist, f_lie, f_lean, f_loft, f_raw }

-- Hook into the standard Bluetooth Attribute Protocol (ATT) fields
local f_btatt_value = Field.new("btatt.value")
local f_btatt_opcode = Field.new("btatt.opcode")

function vertex_proto.dissector(tvb, pinfo, tree)
    local att_opcode = f_btatt_opcode()
    local att_value = f_btatt_value()

    -- 0x1b is Handle Value Notification (The typical GATT push from sensor to phone)
    if att_opcode and att_opcode.value == 0x1b and att_value then
        local val_tvb = att_value.range
        local length = val_tvb:len()

        -- We hypothesize a payload of at least 12 bytes based on the 6 metrics we found.
        -- If it's a notification with a decent payload size, we attempt to dissect it.
        if length >= 12 then
            pinfo.cols.protocol = "VERTEX"
            pinfo.cols.info = "Vertex Golf Swing Data Detected!"

            local subtree = tree:add(vertex_proto, val_tvb(), "Vertex Golf Sensor Payload")
            subtree:add(f_raw, val_tvb(0, length))
            
            -- Assuming Little-Endian packing (standard for ARM Cortex BLE devices)
            subtree:add_le(f_speed, val_tvb(0, 2))
            subtree:add_le(f_face, val_tvb(2, 2))
            subtree:add_le(f_twist, val_tvb(4, 2))
            subtree:add_le(f_lie, val_tvb(6, 2))
            subtree:add_le(f_lean, val_tvb(8, 2))
            subtree:add_le(f_loft, val_tvb(10, 2))
        end
    end
end

-- Register as a post-dissector so it runs AFTER Wireshark decrypts the BLE frame
register_postdissector(vertex_proto)
