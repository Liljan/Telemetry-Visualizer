require "telemetry_visualizer/telemetry_viewport_toolbar"
require "core/editor_slave/stingray_editor/gizmo_manager"

local TelemetryEditorViewportBehavior = class(TelemetryEditorViewportBehavior)

function TelemetryEditorViewportBehavior:init(id, editor, window)
    self._id = id
    self._event_handlers = {}
    self._objects = {}
    self._editor = editor
    self._level_editing = editor:level_editing()
    self._does_accept_dnd = false
    self._window = window
    self._world = Application.new_world()
    self._viewport = Application.create_viewport(self._world, "default")
    self._editor_camera = EditorCamera.create_viewport_editor_camera(id, self._world, {"QuakeStyleMouseLook"})
    self._shading_environment = World.create_shading_environment(self._world)
    self._is_dirty = true
    self._grid = self._level_editing.grid
    self._fields =  {}
    self._visualization_modes = { NON = 1, POINTCLOUD = 2, POINTCLOUD_COLOR = 3 }
    self._visualization_mode = self._visualization_modes.NON --Default mode
    self._parser_modes = { NON = 0, VECTOR3 = 1, POSITION = 2 }
    self._parser_mode = self._parser_modes.NON

    if self._window then
        -- Required by EditorViewport
        self._toolbar_behavior = TelemetryViewportToolbarBehavior()
        self._toolbar_behavior:hide_root_motion_disabled_message()
    end

    -- Easy way to manage background level
    self._gizmo_manager = GizmoManager()

    self:on("load_background_level")
    self:on("create_entity")
    self:on("create_entities")
    self:on("create_or_update_entity")
    self:on("destroy_entitiy")
    self:on("destroy_entities")
    self:on("reset_entity")
    self:on("remove_component")
    self:on("destroy_entity")
    self:on("set_component_property")
    self:on("visualize_point_cloud")
end

function TelemetryEditorViewportBehavior:on(method_name)
    local off_callback_handler = self._editor:on(self._id, method_name, Func.method(method_name, self))
    self._event_handlers[method_name] = off_callback_handler
    return off_callback_handler
end

function TelemetryEditorViewportBehavior:off(method_name, callback)
    self._event_handlers[method_name] = nil
    self._editor:off(self._id, method_name, callback)
end

-------------------------------------
-- Parse an array of strings to Vector3
-- @param positions Array of strings.
-------------------------------------
local function special_string_to_vec3(positions)

    local result = {}

    for i=1, #positions do
        local val1, val2, val3
        local position = string.sub(positions[i], string.len("Vector3(") + 1)

        local x = string.find(position, ",")
        if x ~= nil  then
            val1 = string.sub(position, 0, x - 1)
            position = string.sub(position, string.len(val1) + string.len(", ") + 1)
        end        

        local y = string.find(position, ",")
        if y ~= nil  then
            val2 = string.sub(position, 0, y - 1)
            position = string.sub(position, string.len(val2) + string.len(", ") + 1)
        end

        val3 = string.sub(position, 0, string.len(position) - 1)
        
        val1_num, val2_num, val3_num = tonumber(val1), tonumber(val2), tonumber(val3) 

        if type(val1_num) == "nil" or type(val2_num) == "nil" or type(val3_num) == "nil"  then
            result[i] = nil
        else
            result[i] = Vector3(val1_num, val2_num, val3_num)
        end
    end

    return result
end

-------------------------------------
-- Parse an array of string to Vector3
-- @param positions Array of strings.
-------------------------------------
local function testdb_string_to_vec3(positions)

    local result = {}

    for i=1, #positions do
        local val1, val2, val3
        local position = string.sub(positions[i], string.len("position(") + 2)

        local x = string.find(position, ",")
        if x ~= nil  then
            val1 = string.sub(position, 0, x - 1)
            position = string.sub(position, string.len(val1) + string.len(", ") + 1)
        end
        
        local y = string.find(position, ",")
        if y ~= nil  then
            val2 = string.sub(position, 0, y - 1)
            position = string.sub(position, string.len(val2) + string.len(", ") + 1)
        end

        val3 = string.sub(position, 0, string.len(position) - 1)

        val1_num, val2_num, val3_num = tonumber(val1), tonumber(val2), tonumber(val3) 

        if type(val1_num) == "nil" or type(val2_num) == "nil" or type(val3_num) == "nil" then
            result[i] = nil
        else
            result[i] = Vector3(val1_num, val2_num, val3_num)
        end
    end

    return result
end


-------------------------------------
-- Write your own parser if you have a different string layout
-- @param positions Array of strings.
-------------------------------------
--[[local function exampel_parser(positions)
    local result = {}

    for i=1, #positions do
        --Parse string
    end

    return result
end
]]

local function parse_data(data, parser_mode)

local data_output = nil

if parser_mode == 0 then
    data_output = data
elseif parser_mode == 1 then
    data_output = testdb_string_to_vec3(data)
elseif parser_mode == 2 then
    data_output = special_string_to_vec3(data)
end

return data_output

end

-------------------------------------
-- Parse an array of strings to Vector3
-- @param positions Array of strings.
-------------------------------------
local function render_point_cloud(lines, fields, data_mode)

    local positions = parse_data(fields.positions, data_mode)

    local alpha = 255
    local color_value = 255

    for i=1, #positions do
        local position = positions[i]

        if position ~= nil then
            LineObject.add_box(lines, Color(alpha, color_value, color_value, color_value), Matrix4x4.from_translation(position), Vector3(1.0,1.0,1.0))
        end
    end    
end

-------------------------------------
-- Render a point_cloud of boxes with a color. Red (bad) - black (OK) - green (good) 
-- @param lines, A line object
-- @param fields, Arrays of postions and scalar values 
-------------------------------------
local function render_point_cloud_scale(lines, fields, data_mode)

    local positions = parse_data(fields.positions, data_mode)

    local scalars = fields.scalars
    local alpha = 255
    local color_value_default = 0

    for i=1, #positions do
        local position = positions[i]

        if position ~= nil then
            local scalar_value = scalars[i]
            local color_scale_value

            if type(scalar_value) == "nil" or type(scalar_value) ~= "number" then
                color_value_default, color_scale_value = 125, 125
            elseif scalar_value > fields.desired_min then 
                color_scale_value = 255*((scalar_value - fields.desired_min) / (fields.max - fields.desired_min)) --Normalize values between 0 and 255
                LineObject.add_box(lines, Color(alpha, color_value_default, color_scale_value, color_value_default), Matrix4x4.from_translation(position), Vector3(1.0, 1.0, 1.0)) --red to black color scale
            elseif scalar_value < fields.desired_min then 
                color_scale_value = 255 - 255*((scalar_value - fields.min) / (fields.desired_min - fields.min)) --Normalize values between 255 and 0
                LineObject.add_box(lines, Color(alpha, color_scale_value, color_value_default, color_value_default), Matrix4x4.from_translation(position), Vector3(1.0, 1.0, 1.0)) --red to black color scale
            end
        end
    end
end


function TelemetryEditorViewportBehavior:render(editor_viewport, lines, lines_no_z)
    if self._shading_environment ~= nil then
        ShadingEnvironment.blend(self._shading_environment, {"default", 1.0})
        ShadingEnvironment.apply(self._shading_environment)
    end

    if self._grid.is_visible_at_origin and editor_viewport:is_grid_visible() then
        Drawing.draw_world_origin_grid(lines, self._grid.stepsize, self._grid.worldsize)
    end

    World.clear_permanent_lines(self._world)

    if self._visualization_mode == self._visualization_modes.POINTCLOUD then
        render_point_cloud(lines_no_z, self._fields, self._parser_mode)
    elseif self._visualization_mode == self._visualization_modes.POINTCLOUD_COLOR then
        render_point_cloud_scale(lines_no_z, self._fields, self._parser_mode)
    end
    
    LineObject.dispatch(self._world, lines)
    LineObject.dispatch(self._world, lines_no_z)

    LineObject.reset(lines);

    if self._window ~= nil then
        Application.render_world(self._world, self._editor_camera:camera(), self._viewport, self._shading_environment, self._window)
    end
end

function TelemetryEditorViewportBehavior:is_accepting_drag_and_drop() return self._does_accept_dnd end
function TelemetryEditorViewportBehavior:world() return self._world end
function TelemetryEditorViewportBehavior:editor_camera() return self._editor_camera end
function TelemetryEditorViewportBehavior:selected_units() return {} end
function TelemetryEditorViewportBehavior:shading_environment() return self._shading_environment end

-- Optional
function TelemetryEditorViewportBehavior:update(editor_viewport, dt)
    self._gizmo_manager:update_gizmos(self._editor_camera:camera())
    World.update(self._world, dt)
end

function TelemetryEditorViewportBehavior:shutdown()

    self._gizmo_manager:clear()

    self:off(self._id, "load_background_level")
    self:off("visualize_point_cloud")

    Application.destroy_viewport(self._world, self._viewport)
    World.destroy_shading_environment(self._world, self._shading_environment)
    Application.release_world(self._world)
end

function TelemetryEditorViewportBehavior:reset()-- Called at editor start and at every level change.
    self._viewport = Application.create_viewport(self._world, "default")
end

function TelemetryEditorViewportBehavior:is_dirty() return self._is_dirty end
function TelemetryEditorViewportBehavior:grid() return self._grid end
function TelemetryEditorViewportBehavior:toolbar_behavior() return self._toolbar_behavior end
function TelemetryEditorViewportBehavior:set_skydome_unit(unit)
    if self._skydome_unit then
        World.destroy_unit(self._world, self._skydome_unit)
        self._skydome_unit = nil
    end

    if unit ~= "" then
        self._skydome_unit = World.spawn_unit(self._world, unit)
    end
end
function TelemetryEditorViewportBehavior:set_shading_environment(shading_environment)
    World.set_shading_environment(self._world, self._shading_environment, shading_environment)
end
function TelemetryEditorViewportBehavior:pre_render(viewport)
    self._level_editing:pre_render(viewport)
    self._editor_camera:pre_render()
end
function TelemetryEditorViewportBehavior:post_render(viewport) end

function TelemetryEditorViewportBehavior:load_background_level(level_name)
    if self._skydome_unit then
        World.destroy_unit(self._world, self._skydome_unit)
        self._skydome_unit = nil
    end

    if self._background_level ~= nil then
        self._gizmo_manager:unregister_level(self._background_level)
        World.destroy_level(self._world, self._background_level)
        self._background_level = nil, nil
    end

    self._background_level = World.load_level(self._world, level_name)
    Level.spawn_background(self._background_level)
    Level.trigger_level_loaded(self._background_level)

    self._gizmo_manager:register_level(self._background_level)

    if Level.has_data(self._background_level, "shading_environment") then
        World.set_shading_environment(self._world, self._shading_environment, Level.get_data(self._background_level, "shading_environment"))
    end

        --Disable flow in level
    World.set_flow_enabled(self._world, false)
    World.clear_permanent_lines(self._world)
    World.set_editor_flow_enabled(self._world, false)

    return self._background_level
end

function TelemetryEditorViewportBehavior:create_entity(entity_id, component_properties_by_id)
    return EntitySystem:create_entity(self._world, entity_id, component_properties_by_id)
end

function TelemetryEditorViewportBehavior:create_entities(entities)
    for index, entity in pairs(entities) do
        for entity_id, component_properties_by_id in pairs(entity) do
            local new_entity = EntitySystem:create_entity(self._world, entity_id, component_properties_by_id)
            self._objects[entity_id] = new_entity
        end
    end
end

function TelemetryEditorViewportBehavior:reset_entity(entity_id, component_properties_by_id)
    return EntitySystem:reset_entity(entity_id, component_properties_by_id)
end

function TelemetryEditorViewportBehavior:destroy_entity(entity_id)
    EntitySystem:destroy_entity(entity_id)
end

function TelemetryEditorViewportBehavior:destroy_entities(entity_ids)
    for index, entity_id in pairs(entity_ids) do
        EntitySystem:destroy_entity(entity_id)
    end
end

function TelemetryEditorViewportBehavior:set_component_property(entity_id, component_id, property_path, property_value)
    EntitySystem:set_component_property(entity_id, component_id, property_path, property_value)
end

function TelemetryEditorViewportBehavior:create_or_update_entity(entity_id, component_properties_by_id)
    local entity = self._objects[entity_id]
    if entity then
        entity = self:reset_entity(entity_id, component_properties_by_id)
    else
        entity = self:create_entity(entity_id, component_properties_by_id)
end
    self._objects[entity_id] = entity
end

-------------------------------------
-- Assign arrays to database fields values and sets a visualization mode
-- @param positions, Array of positions.
-- @param scalars, Array of scalars.
-- @param min, Minimum value when appyling the color scale.
-- @param medium, The value to differentiate "good" and "bad"" values when appyling the color scale.
-- @param max, Maximum value when appyling the color scale.
-------------------------------------
function TelemetryEditorViewportBehavior:visualize_point_cloud(parser_mode, positions, scalars, min, desired_min, max)    
   
    self._fields = {} --Reset table

    self._fields["positions"] = positions

    --different modes. With or without scalar values
    if(scalars == nil) then
        self._visualization_mode = self._visualization_modes.POINTCLOUD
    else
        self._visualization_mode = self._visualization_modes.POINTCLOUD_COLOR
        self._fields["scalars"] = scalars
        self._fields["min"] = min
        self._fields["desired_min"] = desired_min
        self._fields["max"] = max
    end

    if parser_mode == 0 then
        self._parser_mode = self._parser_modes.NON
    elseif parser_mode == 1 then
        self._parser_mode = self._parser_modes.VECTOR3
    elseif parser_mode == 2 then
        self._parser_mode = self._parser_modes.POSITION
    end
end

return TelemetryEditorViewportBehavior
