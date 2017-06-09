require "core/editor_slave/stingray_editor/button_toggle_behavior"
require "core/editor_slave/stingray_editor/button_normal_behavior"
require "core/editor_slave/stingray_editor/textured_button_renderer"
require "core/editor_slave/stingray_editor/hover_textured_button_renderer"
require "core/editor_slave/stingray_editor/text_button_renderer"
require "core/editor_slave/stingray_editor/text_message_renderer"

TelemetryViewportToolbarBehavior = class(TelemetryViewportToolbarBehavior)

function TelemetryViewportToolbarBehavior:init()
end

function TelemetryViewportToolbarBehavior:create_widgets(viewport_id, gui, window_width, window_height)
    assert(Validation.is_guid(viewport_id), "viewport_id must be a UUID 4 string")
    assert(gui ~= nil, "gui is nil")
    assert(Validation.is_non_negative_integer(window_width), "window_width must be a non-negative integer")
    assert(Validation.is_non_negative_integer(window_height), "window_height must be a non-negative integer")

    self._viewport_id = viewport_id
    self._gui = gui
    self._window_width = window_width
    self._window_height = window_height

    self._position_using_window_size = function(positioner, button_width, button_height, parent)
        local x, y = positioner(window_width, window_height, button_width, button_height, parent)
        return x, y
    end

    -- default height is taken from a string containing lower case, upper case and space to get the max height possible. Both buttons are force to that height.
    local dummy, default_button_height = TextButtonRenderer.compute_button_size(self._gui, "Full Render", "core/editor_slave/resources/gui/open_sans_16", 16)
    local get_button_size = function (gui, text, font, font_size)
        local w, h = TextButtonRenderer.compute_button_size(gui, text, font, font_size)
        h = default_button_height;
        return w, h
    end

    self._options_button = Button(gui,
        NormalButtonBehavior(Func.method("_viewport_and_level_options_button_clicked", self), "core/editor_slave/resources/gui/options"),
        TexturedButtonRenderer(20, 20),
        ButtonPositioner.TopLeft,
        self._position_using_window_size)

    self._viewport_cameras_button = Button(gui,
        NormalButtonBehavior(Func.method("_viewport_cameras_button_clicked", self), "core/editor_slave/resources/gui/open_sans_16"),
        TextButtonRenderer(gui, "core/editor_slave/resources/gui/open_sans_16", "Persp", 16, get_button_size),
        ButtonPositioner.RightOfParent,
        self._position_using_window_size,
        self._options_button)

    self._visualization_modes_button = Button(gui,
        NormalButtonBehavior(Func.method("_visualization_modes_button_clicked", self), "core/editor_slave/resources/gui/open_sans_16"),
        TextButtonRenderer(gui, "core/editor_slave/resources/gui/open_sans_16", "Full Render", 16, get_button_size),
        ButtonPositioner.RightOfParent,
        self._position_using_window_size,
        self._viewport_cameras_button)

    self._anim_options_button = Button(gui,
        NormalButtonBehavior(Func.method("_grid_options_button_clicked", self), "core/editor_slave/resources/gui/open_sans_16"),
        TextButtonRenderer(gui, "core/editor_slave/resources/gui/open_sans_16", "View", 16, get_button_size),
        ButtonPositioner.RightOfParent,
        self._position_using_window_size,
        self._visualization_modes_button)

    self._is_root_motion_inactive_button = Button(gui,
        NormalButtonBehavior(Func.method("_root_motion_inactive_clicked", self), "core/editor_slave/resources/gui/open_sans_16"),
        TextMessageRenderer(gui, "core/editor_slave/resources/gui/open_sans_16", "Root Motion Disabled", 16, get_button_size),
        ButtonPositioner.RightOfParent,
        self._position_using_window_size,
        self._anim_options_button)

    self._widgets = { self._options_button, self._viewport_cameras_button, self._visualization_modes_button, self._anim_options_button, self._is_root_motion_inactive_button }
    return self._widgets
end

function TelemetryViewportToolbarBehavior:visible()
    return true
end

function TelemetryViewportToolbarBehavior:display_root_motion_disabled_message()
    if self._is_root_motion_inactive_button ~= nil then
        self._is_root_motion_inactive_button:set_visible(true)
    end
end

function TelemetryViewportToolbarBehavior:hide_root_motion_disabled_message()
    if self._is_root_motion_inactive_button ~= nil then
        self._is_root_motion_inactive_button:set_visible(false)
    end
end

function TelemetryViewportToolbarBehavior:_viewport_and_level_options_button_clicked()
    Application.console_send { type = "show_Telemetry_viewport_and_level_options", viewport_id = self._viewport_id, asset = self._resource_path, hideContextualMenu = {HideBackgroundLevel = true, HideAudioRender = true} }
end

function TelemetryViewportToolbarBehavior:_visualization_modes_button_clicked()
    Application.console_send { type = "show_Telemetry_viewport_visualization_modes", viewport_id = self._viewport_id }
end

function TelemetryViewportToolbarBehavior:_grid_options_button_clicked()
    Application.console_send { type = "show_Telemetry_viewport_options", viewport_id = self._viewport_id }
end

function TelemetryViewportToolbarBehavior:_viewport_cameras_button_clicked(x, y)
    Application.console_send { type = "show_Telemetry_viewport_cameras", viewport_id = self._viewport_id, x = x, y = y }
end

function TelemetryViewportToolbarBehavior:window_resize(width, height)
    -- Show toolbar if previously hidden in split_widget_clicked. Avoids glitches.
    self._window_width = width
    self._window_height = height
    self._visible = true
end

function TelemetryViewportToolbarBehavior:_relayout()
    for _, widget in ipairs(self._widgets) do
        widget:window_resize(self._window_width, self._window_height)
    end
end

function TelemetryViewportToolbarBehavior:set_viewport_camera_display_name(name)
    if self._visualization_modes_button ~= nil then
        self._viewport_cameras_button:renderer():set_text(self._gui, name)
        self:_relayout()
    end
end

function TelemetryViewportToolbarBehavior:set_visualization_mode_name(name)
    if self._visualization_modes_button ~= nil then
        self._visualization_modes_button:renderer():set_text(self._gui, name)
        self:_relayout()
    end
end

