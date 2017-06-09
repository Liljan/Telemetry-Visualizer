define((require) => {
    'use strict';

    const assert = require('entity-editor/src/js/base/assert');
    const engineService = require('services/engine-service');
    const engineViewportService = require('services/engine-viewport-service');
    const hostService = require('services/host-service');
    const marshallingService = require('services/marshalling-service');
    const marshallEntityForLua = require('entity-editor/src/js/marshall-entity-for-lua');

    const projectService = require('services/project-service');
    const contentDatabaseService = require('services/content-database-service');
    const DefaultViewportMouseBehavior = require('common/default-viewport-mouse-behavior');
    const DefaultViewportController = require('common/default-viewport-controller');
    const ViewportContextMenus = require('core/views/viewport-context-menus');

    class TelemetryViewportMouseBehavior extends DefaultViewportMouseBehavior {
        mouseDown (e, viewportId, x, y) {
            const buttonNumber = e.button;
            DefaultViewportMouseBehavior.modifiersFromMouseEvent(e);

            switch (buttonNumber) {
                case 0:
                    this.setCameraControlStyle('MayaStyleTurntableRotation');
                    this._setCursor('ew-resize');
                    this.engineViewportInterops.mouseLeftDown(viewportId, x, y);
                    break;
                case 1:
                    this.setCameraControlStyle('MayaStylePan');
                    this._setCursor('move');
                    this.engineViewportInterops.mouseMiddleDown(viewportId, x, y);
                    break;
                case 2:
                    this.setCameraControlStyle('MayaStyleDolly');
                    this._setCursor('ns-resize');
                    this.engineViewportInterops.mouseRightDown(viewportId, x, y);
                    break;
            }
        }

        mouseUp (e, viewportId, x, y) {
            super.mouseUp(e, viewportId, x, y);
            this._setCursor('default');
        }

        mouseMove (e, viewportId, x, y, deltaX, deltaY) {
            e.preventDefault();
            this.engineViewportInterops.mouseMove(viewportId, x, y, deltaX, deltaY);
        }

        _setCursor (cursor) {
            let $body = $('body');
            $body.css('cursor', cursor);
        }
    }

    class ViewportController extends DefaultViewportController {
        constructor() {
            super(engineService);
        }


        setup (engineViewportId, engineViewportInterops) {
            super.setup(engineViewportId, engineViewportInterops);
            let off = null;

            engineViewportService.setFocusedViewportId(this.viewportId);
            this.setMouseBehavior(new TelemetryViewportMouseBehavior(engineService, engineViewportId, engineViewportInterops));

            window.addEventListener('focus', () => {
                engineViewportService.setFocusedViewportId(this.viewportId);
            }, false);


            return new Promise((resolve) => {
                off = engineViewportService.on('ViewportCreated', (id) => {
                    if (id === this.viewportId) {
                        resolve();
                    }
                });
                engineViewportService.getViewportNameFromId(this.viewportId).then(name => {
                    if (name) {
                        // Create the viewport contextual menus to work with the viewport toolbar created in lua
                        this.viewportContextMenus = this.createContextMenus(name);
                        this.engineMessageHandlers = this.createEngineMessageHandlers(this.viewportContextMenus);
                        this.viewportContextMenus.setMenuVisibilities({PlaybackRate: false, ResetToggles: false});
                        resolve();
                    }
                });
            }).then(() => {
                //return this.loadBackgroundLevel('core/editor_slave/resources/levels/empty_level');
                return engineViewportInterops.raise(engineViewportId, 'load_background_level', 'core/editor_slave/resources/levels/empty_level');
            }).then(() => {
                if (off)
                    off();
                return this;
            });
        }

        createContextMenus(name) {
            return new ViewportContextMenus(
                {
                    engineService: engineService,
                    engineViewportService: engineViewportService,
                    hostService: hostService,
                    marshallingService: marshallingService,
                    projectService: projectService,
                    contentDatabaseService: contentDatabaseService
                }, this.viewportId, name, null, null);
        }

        createEngineMessageHandlers(viewportContextMenus) {
            return [
                engineService.addEditorEngineMessageHandler(
                    'show_entity_viewport_and_level_options',
                    viewportContextMenus.handleShowViewportAndLevelOptions.bind(viewportContextMenus)
                ),
                engineService.addEditorEngineMessageHandler(
                    'show_entity_viewport_cameras',
                    viewportContextMenus.handleShowViewportCamerasMenu.bind(viewportContextMenus)
                ),
                engineService.addEditorEngineMessageHandler(
                    'show_entity_viewport_visualization_modes',
                    viewportContextMenus.handleShowVisualizationModesMenu.bind(viewportContextMenus)
                ),
                engineService.addEditorEngineMessageHandler(
                    'show_entity_viewport_options',
                    viewportContextMenus.handleShowGridContextMenu.bind(viewportContextMenus)
                )
            ];
        }

        showSettingsContextMenu() {
            if (!this.viewportContextMenus)
                throw new Error('You need to run setup first');

            this.viewportContextMenus.showSettingsContextMenu();
        }

        raise() {
            if (!this.engineViewportInterops)
                throw new Error('You need to run setup first');

            let args = [this.viewportId].concat(Array.prototype.slice.call(arguments, 0));
            return this.engineViewportInterops.raise.apply(this, args);
        }

        createDenormalizeMethods(store) {
            this.denormalizeEntity = (entity) => marshallEntityForLua.denormalizeEntity(store, entity);
            this.denormalizeEntities = (entities) => marshallEntityForLua.denormalizeEntities(store, entities);
        }

        createEntity(store, entity) {
            assert.isDefined(this.denormalizeEntity, 'Denormalize methods not created - run createDenormalizeMethods first');
            return this.raise('create_entity', entity.id, this.denormalizeEntity(entity));
        }

        createEntities(entities) {
            assert.isDefined(this.denormalizeEntity, 'Denormalize methods not created - run createDenormalizeMethods first');
            const denormalizedEntities = this.denormalizeEntities(entities);
            return this.raise('create_entities', denormalizedEntities);
        }

        createOrUpdateEntity(entity) {
            assert.isDefined(this.denormalizeEntity, 'Denormalize methods not created - run createDenormalizeMethods first');
            return this.raise('create_or_update_entity', entity.id, this.denormalizeEntity(entity));
        }

        destroyEntity(entityId) {
            return this.raise('destroy_entity', entityId);
        }

        destroyEntities(entityIds) {
            return this.raise('destroy_entities', entityIds);
        }

        setComponentProperty(entityId, componentId, propertyPath, propertyValue) {
            return this.raise('set_component_property', entityId, componentId, propertyPath, propertyValue);
        }

        resetEntity(entity) {
            assert.isDefined(this.denormalizeEntity, 'Denormalize methods not created - run createDenormalizeMethods first');
            return this.raise('reset_entity', entity.id, this.denormalizeEntity(entity));
        }

        loadBackgroundLevel(level) {
            return this.raise('load_background_level', level);
        }

    }

    return ViewportController;
});