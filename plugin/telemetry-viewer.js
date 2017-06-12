define(function (require) {
    'use strict';

    const stingray = require('stingray');

    const hostService = require('services/host-service');
    const eventService = require('services/event-service');

    const props = require('properties/property-editor-utils');
    require('properties/property-models');

    const m = require('components/mithril-ext');
    const EngineViewport = require('components/engine-viewport');
    const Textbox = require('components/textbox');
    const Button = require('components/button');
    const Toolbar = require('components/toolbar');
    const Choice = require('components/choice');
    const Checkbox = require('components/checkbox');
    require('components/resizer');
    const Accordion = require('components/accordion');
    const Spinner = require('components/spinner');
    const Resource = require('components/resource');
    const ListView = require('components/list-view');

    const DEFAULT_IP = 'localhost';
    const DEFAULT_PORT = '27017';
    const DEFAULT_DB = '';
    const DEFAULT_DLL_PATH = 'telemetry_visualizer/binaries/editor/win64/release/editor_plugin_w64_release.dll';

    const Visualizations = {
        POINTCLOUD: 1,
        // More visualization types can be added here.
    }

    const Parsers = {
        NO_PARSER: 0, 
        VECTOR3: 1,
        POSITION: 2,
        // More Parsers can be added here.
    }

    class TelemetryViewer {

        constructor() {

            // ---- VIEWPORT VARIABLES ----
            this.viewportHandle = EngineViewport.config({
                name: "telemetry-viewport",
                clearOnWindowUnload: false
            });

            // ---- DATABASE VARIABLES ----
            this.selectedMode = Parsers.VECTOR3;
            this.prefix = 'mongodb://';

            this.pluginId = this.loadNativeExtension();

            this.selectedCollection = null;
            this.collections = null;

            this.ip = m.prop(DEFAULT_IP);
            this.port = m.prop(DEFAULT_PORT);
            this.dataBase = m.prop(DEFAULT_DB);

            this.databaseAccordion = Accordion.component([
                {
                    title: "Database options",
                    isExpanded: true,
                    collapsible: true,
                    content: () => {
                        return [Toolbar.component({
                            items: [
                                { component: this.prefix },
                                { component: Textbox.component({ model: this.ip, placeholder: "IP number", clearable: true }) },
                                { component: ":" },
                                { component: Textbox.component({ model: this.port, placeholder: "Port number", clearable: true }) },
                                { img: 'tab_close_normal.svg', title: 'Clear', action: () => { this.ip(''), this.port('') } },
                                { img: 'undo.svg', title: 'Default', action: () => { this.ip(DEFAULT_IP), this.port(DEFAULT_PORT) } },
                                { img: 'play.svg', title: 'Connect', action: () => this.connect() }
                            ]
                        }),
                        Toolbar.component({
                            items: [
                                { component: "Database" },
                                { component: Textbox.component({ model: this.dataBase, placeholder: "Database name", clearable: true }) },
                                { img: 'tab_close_normal.svg', title: 'Clear', action: () => { this.dataBase('') } },
                                { img: 'undo.svg', title: 'Default', action: () => { this.dataBase(DEFAULT_DB) } },
                                { img: 'play.svg', title: 'Connect', action: () => this.selectDatabase() }
                            ]
                        })];
                    }
                }
            ]);

            // ---- POSITION PARSER SPECIFIC VARIABLES ----

            this.POSITION_Accordion = null;

            // ---- PARSER MODE VARIABLES ----

            this.setMode = (option) => {
                switch (option) {
                    case Parsers.POSITION:
                        this.selectedMode = Parsers.VECTOR3;
                        this.positionParserAccordion = null;
                        break;
                    case Parsers.VECTOR3:
                        this.selectedMode = Parsers.POSITION;
                        this.levelKey = m.prop("level_key");

                        this.positionParserAccordion = Accordion.component([{
                            title: "Level key",
                            content: () => {
                                return [
                                    Toolbar.component({
                                        items: [
                                            { component: "Level key" },
                                            { component: Textbox.component({ model: this.levelKey, placeholder: "level_key" }) },
                                        ]
                                    })
                                ];
                            }
                        }]);

                        break;

                    default: 
                    this.selectedMode = Parsers.NO_PARSER;
                    this.positionParserAccordion = null;
                    break;
                }
            }

            this.selectModeModel = m.helper.modelWithTransformer(m.prop(0), null, (viewStrValue) => {
                let parsed = parseInt(viewStrValue);

                this.setMode(parsed);

                return parsed;
            });

            this.modeOptions = () => {
                return {
                    'Default': Parsers.NO_PARSER,
                    'Position(x, y, z)': Parsers.POSITION,
                    'Vector3(x, y, z) (special database fetch)': Parsers.VECTOR3,
                    // more options can be added here
                };
            };

            this.selectModeAccordion = Accordion.component(
                [{
                    title: "Parser for visualization attribute 'Position'",
                    collapsible: true,
                    content: () => {
                        return Toolbar.component({
                            items: [
                                { component: "Parser: " },
                                { component: Choice.component({ model: this.selectModeModel, getOptions: this.modeOptions }) }
                            ]
                        });
                    }
                }]);

            // ---- LEVEL VARIABLES ----

            this.resourceModel = m.prop('content/levels/basic.level');

            this.levelAccordion = Accordion.component([
                {
                    title: "Load level",
                    collapsible: true,
                    content: () => {
                        return Toolbar.component({
                            items: [{
                                component: Resource.component({
                                    model: this.resourceModel, resourceType: 'level', hasResourceSelect: true
                                })
                            },
                            {
                                component: Button.component({ text: "Load level", onclick: () => this.loadLevel() })
                            }]
                        });
                    }
                }
            ]);

            // ------- FIELDS VARIABLES ------

            this.selectCollectionComponent = null;

            /**
             * Behavior of the drop-down component for selecting
             * what collection to display fields from.
             * @param {number} item
             * @return {number}
             */
            this.selectCollectionModel = item => {
                if (!_.isNil(item)) {

                    if (item !== "null" && item !== "undefined") {
                        this.selectedCollection = this.collections[item];

                        let fieldNames = window.nativeExtension.fetchFieldKeys(this.selectedCollection);

                        this.fieldConfig.items = [];

                        fieldNames.forEach(v => this.fieldConfig.items.push({
                            isIncluded: false,
                            isSorted: false,
                            field: m.prop(v),
                        }));

                        this.fieldDiv = m('div', { className: 'fullscreen entity-editor-property-container' },
                            m('div', { key: id.toString(), style: 'height:150px;' }, [
                                ListView.component(this.fieldConfig)
                            ])
                        );

                        m.redraw(this.fieldAccordion);
                    }
                }
                return item;
            };

            this.fetchSkip = m.prop(0);
            this.fetchLimit = m.prop(1000);

            this.fetchDataButton = Button.component({
                text: "Fetch data", onclick: () =>
                    this.fetchDocuments(this.selectedCollection, { limit: this.fetchLimit() }, { skip: this.fetchSkip() },
                        { fields: this.getIsIncludedFields() }, { sort: this.getSortFields() }), disabled: true
            });

            this.fieldColumns = [
                {
                    uniqueId: "isIncludedCheckbox",
                    type: m.column.checkbox,
                    header: { text: "Include", tooltip: "Sort by Include" },
                    property: "isIncluded",
                    tooltipProperty: "isIncluded",
                    disabled: function (item) {
                        return (item) ?
                            (m.utils.isFunction(item.isReadonly) ? item.isReadonly() : item.isReadonly) :
                            false;
                    }
                },
                {
                    uniqueId: "isSortedCheckbox",
                    type: m.column.checkbox,
                    header: { text: "Sort by", tooltip: "Should sort by" },
                    property: "isSorted",
                    tooltipProperty: "isSorted",
                    disabled: function (item) {
                        return (item) ?
                            (m.utils.isFunction(item.isReadonly) ? item.isReadonly() : item.isReadonly) :
                            false;
                    }
                },
                {
                    uniqueId: "field",
                    header: { text: "Field", tooltip: "Sort by field" },
                    property: "field",
                    tooltipProperty: "field",
                    editable: false,
                },
            ];

            this.fieldConfig = ListView.config({
                items: [],
                columns: this.fieldColumns,
                layoutOptions: ListView.toLayoutOptions({
                    size: "3",
                    filter: ""
                }),

                defaultSort: { uniqueId: "field", property: "field", reverse: false },
                showHeader: true,
                showItemFocus: false,
                showLines: true,
                allowSort: true,
                allowMultiSelection: true,
                allowColumnResize: true,
                allowArrowNavigation: true,
                allowTypedNavigation: true,
                allowClearSelection: true,
                selectionCanBeEmpty: true,
                filterProperty: "*",
            });

            this.fieldDiv = null;

            this.fieldAccordion = Accordion.component([
                {
                    title: "Fields to include",
                    isExpanded: true,
                    content: () => {

                        return [
                            this.selectCollectionComponent,
                            this.fieldDiv,

                            Toolbar.component({
                                items: [
                                    { component: "Skip to" },
                                    {
                                        component: Spinner.component({
                                            model: this.fetchSkip,
                                            min: 0,
                                            max: 100000,
                                            increment: 1,
                                            showLabel: false,
                                            decimal: 0,
                                        })
                                    },
                                    { component: "Amount" },
                                    {
                                        component: Spinner.component({
                                            model: this.fetchLimit,
                                            min: 0,
                                            max: 100000,
                                            increment: 1,
                                            showLabel: false,
                                            decimal: 0,
                                        })
                                    },
                                    { component: this.fetchDataButton }
                                ]
                            })];
                    }
                }
            ]);

            // ------ DATABASE DOCUMENTS VARIABLES ------

            this.documentDiv = null;

            this.documentAccordion = Accordion.component([{
                title: "Documents",
                isExpanded: true,
                content: () => {
                    return [this.documentDiv];
                }
            }]);

            // ------ VISUALIZATION DATA ------

            this.visualizeButton = Button.component({ text: "Visualize", onclick: () => this.visualize(), disabled: true });

            this.visualizationOptions = () => {
                return {
                    'Point Cloud': Visualizations.POINTCLOUD,
                    // more options can be added here
                };
            };

            this.pointCloud = new PointCloud();

            // This variable keeps track of what visualization is chosen and displayed.
            this.activeVisualization = this.pointCloud;

            this.visualizationMethodModel = m.helper.modelWithTransformer(m.prop(Visualizations.POINTCLOUD), null, (viewStrValue) => {
                let chosen = parseInt(viewStrValue);

                switch (chosen) {
                    case Visualizations.POINTCLOUD:
                        console.log("This is a scatter plot");
                        this.activeVisualization = this.pointCloud;
                        break;
                    default:
                        break;
                }

                this.selectedMode = chosen;
                return chosen;
            });

            this.visualizationAccordion = Accordion.component([{
                title: "Visualization",
                collapsible: true,
                isExpanded: true,
                content: () => {
                    return [Choice.component({ model: this.visualizationMethodModel, getOptions: this.visualizationOptions }),
                    this.activeVisualization.component, this.visualizeButton];
                }
            }]);

            window.addEventListener('unload', () => {
                this.unloadNativeExtension(this.pluginId);
                this.viewportHandle.destroyViewport();
            });

            eventService.on('AboutToQuit', () => {
                this.unloadNativeExtension(this.pluginId);
                this.viewportHandle.destroyViewport();
            });
        }

        /**
         * Connects the plugin with the native plugin DLL.
         */
        loadNativeExtension() {
            const nativePluginDllPath = require.toUrl(DEFAULT_DLL_PATH);

            if (!stingray.fs.exists(nativePluginDllPath))
                throw new Error('Telemetry editor native plugin does not exists at `' + nativePluginDllPath + '`. Was it compiled?');

            let pluginId = stingray.loadNativeExtension(nativePluginDllPath);
            return pluginId;
        }

        /**
         * Unloads the native DLL.
         */
        unloadNativeExtension(pluginId) {
            stingray.unloadNativeExtension(pluginId);
        }

        /**
         * Connects the plugin with a mongoDB database.
         */
        connect() {
            let success;
            let newDatabaseAdress = this.prefix + this.ip() + ":" + this.port();

            if (this.ip() && this.port())
                success = window.nativeExtension.connectToDatabase(newDatabaseAdress);
            else
                console.warn("Could not connect to the selected MongoDB.")

            if (success) {
                this.viewportHandle.ready.then((viewportController) => {
                    return viewportController.raise("connect_to_database", newDatabaseAdress);
                });
                this.databaseAdress = newDatabaseAdress;
            }
            else if (this.databaseAdress == newDatabaseAdress) {
                console.info("Already connected.")
            }
            else {
                console.warn("Could not connect to the selected MongoDB/Already connected.")
            }
        }

        /**
         * Behavior of the drop-down component for selecting
         * what collection to display fields from.
         * @param {number} item
         * @return {number}
         */
        selectDatabase() {
            let collectionNames;

            if (this.dataBase())
                collectionNames = window.nativeExtension.selectDatabase(this.dataBase());
            else
                console.warn("Enter a database name");

            if (collectionNames == false) {
                console.warn("Didn't not find a database/Already chosen");
            }
            else {
                this.collections = collectionNames;
                this.selectCollectionComponent = TelemetryViewer.createDropdown(this.selectCollectionModel, this.collections, "Select a collection");

                this.fetchDataButton.attrs.disabled = false;

                this.viewportHandle.ready.then((viewportController) => {
                    return viewportController.raise("select_database", this.dataBase());
                });
            }
        }

        /**
         * Load level in viewport.
         */
        loadLevel() {
            this.viewportHandle.ready.then((viewportController) => {

                let level = this.resourceModel();
                return viewportController.raise("load_background_level", level);
            });
        }

        /**
         * Function that returns an array with field properties which
         * include-checkbox have been marked.
         * @return {Array}
         */
        getIsIncludedFields() {
            let selectedFields = [];

            this.fieldConfig.items.forEach(item => {
                if (item.isIncluded) {
                    selectedFields.push(item.field());
                }
            });

            return selectedFields;
        }

        /**
         * Function that returns an array with field properties which
         * sort-checkbox have been marked.
         * @return {Array}
         */
        getSortFields() {
            let sortFields = [];

            this.fieldConfig.items.forEach(item => {
                if (item.isIncluded) {
                    sortFields.push(item.isSorted);
                }
            });

            return sortFields;
        }

        /**
         * Function that returns an array with documents that have properties which
         * include-checkbox have been marked.
         * @return {Array}
         */
        getSelectedDataFields(key) {
            let values = [];

            this.documentsConfig.items.forEach(item => {

                if (item.isIncluded) {
                    if (item[key] != null)
                        values.push(item[key]);
                }
            });

            return values;
        }

        /**
         * A specialiced function that returns the sessions based on the desired level.
         * Follows the special mode's database structure.
         * @return {Array}
         */
        fetchSessions() {
            return window.nativeExtension.sessionsIds(this.levelKey());
        }

        /**
         * Fetches data from the database.
         * The parameters
         * @return {Array}
         */
        fetchDocuments(collection, limit, skip, fields, sortBy) {

            if (collection == null) {
                console.warn("No collection selected");
                return;
            }

            let fieldLength = fields["fields"].length;

            if (fieldLength == 0) {
                console.warn("No field(s) selected");
                return;
            }

            let documents = null;

            if (this.selectedMode == Parsers.POSITION) {

                let tempSessionIDs = this.fetchSessions();

                /* For some reason the sessionIDs used directly. Another array is used to store
                the sessions instead. */
                let sessionIDs = [];
                tempSessionIDs.forEach(item => { sessionIDs.push(item); });

                let sessions = { sessions_ids: sessionIDs };

                documents = window.nativeExtension.fetchDocuments(collection, skip, limit, fields, sortBy, sessions);
            } else {
                documents = window.nativeExtension.fetchDocuments(collection, skip, limit, fields, sortBy);
            }

            const keys = Object.keys(documents);
            const values = Object.values(documents);

            let numItems = 0;

            for (let value of values)
                numItems = Math.max(numItems, value.length);

            const columns = [{
                uniqueId: "isIncluded",
                type: m.column.checkbox,
                header: { text: "Include", tooltip: "Sort by Include" },
                property: "isIncluded",
                tooltipProperty: "isIncluded",
                disabled: item => false
            }].concat(keys.map(key => ({
                uniqueId: key,
                header: { text: key, tooltip: "Sort by" + key },
                property: key,
            })));

            const documentItems = [];
            for (let i = 0; i < numItems; ++i) {
                let item = { id: i, isIncluded: false };
                for (let k = 0; k < keys.length; ++k)
                    item[keys[k]] = values[k][i];

                documentItems.push(item);
            }

            this.documentsConfig = ListView.config({
                items: documentItems,
                columns: columns,
                layoutOptions: ListView.toLayoutOptions({
                    size: "3",
                    filter: ""
                }),

                showHeader: true,
                showItemFocus: true,
                showLines: true,
                allowSort: true,
                allowMultiSelection: true,
                allowColumnResize: true,
                allowArrowNavigation: true,
                allowTypedNavigation: true,
                allowClearSelection: true,
                selectionCanBeEmpty: true,
                filterProperty: "*",
            });

            this.documentDiv = m('div', { className: 'fullscreen entity-editor-property-container' },
                m('div', { key: Math.random().toString(), style: 'height:300px;' }, [
                    ListView.component(this.documentsConfig)
                ])
            );

            m.redraw(this.documentAccordion);

            // update visualization component
            this.activeVisualization.setFields(fields);
            this.visualizeButton.attrs.disabled = false;
        }

        /**
         * Connect to Lua and send the necessary data for visualization.
         */
        visualize() {

            switch (this.visualizationMethodModel()) {
                case Visualizations.POINTCLOUD:
                    let positions = this.getSelectedDataFields(this.pointCloud.getPositionKey());

                    this.viewportHandle.ready.then((viewportController) => {
                        if (this.pointCloud.useScalar()) {
                            let scalars = this.getSelectedDataFields(this.pointCloud.getScalarKey());
                            viewportController.raise("visualize_point_cloud", this.selectedMode, positions, scalars, this.pointCloud.min(), this.pointCloud.desired(), this.pointCloud.max());
                        } else {
                            viewportController.raise("visualize_point_cloud", this.selectedMode, positions);
                        }
                    });
                    break;

                /**
                 * More visualization types can be regisered here.
                 */

                default:
                    break;
            }
        }

        /**
         * Renders the viewer with all the UI components.
         * @return {view}
         */
        render() {
            return m.layout.vertical({}, [
                m.layout.element({}, [
                    m.resizer.container({ direction: 'horizontal', redrawOnResize: true }, { style: 'width:100%; height:100%;' }, [
                        m.resizer.panel({ 'min-size': 200, ratio: 1 }, [
                            m.resizer.container({ direction: 'vertical', redrawOnResize: true }, { style: 'width:100%; height:100%;' }, [

                                this.databaseAccordion,
                                this.selectModeAccordion,
                                this.positionParserAccordion,
                                this.levelAccordion,
                                this.fieldAccordion,
                                this.documentAccordion,
                                this.visualizationAccordion,
                            ]),
                        ]),
                        m.resizer.panel({ 'min-size': 200, ratio: 1, className: '' }, [
                            EngineViewport.component(this.viewportHandle)
                        ]),
                    ])
                ])
            ]);
        }

        /**
         * Entry point to compose view.
         */
        static view(ctrl) {
            return ctrl.render();
        }

        /**
         * Mount a new playground
         * @param {HtmlElement} container
         * @return {{component, noAngular: boolean}}
         */
        static mount(container) {
            let instance = m.mount(container, m.component({
                controller: () => new TelemetryViewer(),
                view: this.view
            }));
            return { instance, component: this, noAngular: true };
        }

        /**
         * Creates a drop-down box for selecting
         * collection type. Mock data is used for now.
         */
        static createDropdown(ctrl, menuOptions, defaultMessage) {

            return Choice.component({
                model: ctrl,
                useDictValueForLabel: true,
                emptyMessage: "No options available",
                defaultValueName: defaultMessage,
                getOptions: () => {
                    return menuOptions;
                },
            });
        }
    }

    /**
    * Class that handles the UI component and options for
    * a scatter-plot visualization. If you want to create a new
    * visualization type the same structure can be used.
    */

    class PointCloud {

        constructor(fields) {

            let activeFields = null;

            this.useScalar = m.prop(false);
            this.min = m.prop(0);
            this.desired = m.prop(0);
            this.max = m.prop(0);

            let positionModel = m.helper.modelWithTransformer(m.prop(1), null, (viewStrValue) => {
                let parsed = parseInt(viewStrValue);

                return parsed;
            });

            let scalarModel = m.helper.modelWithTransformer(m.prop(1), null, (viewStrValue) => {
                let parsed = parseInt(viewStrValue);

                return parsed;
            });

            this.getPositionKey = () => {
                return activeFields[positionModel()];
            }

            this.getScalarKey = () => {
                return activeFields[scalarModel()];
            }

            let getOptions = () => {
                return activeFields;
            };

            this.setFields = (fields) => {
                activeFields = fields["fields"];
            }

            let checkboxModel = (value) => {
                if (!_.isNil(value)) {
                    if (value) {
                        this.useScalar(true);
                        this.component = [
                            Toolbar.component({ items: positionComponent }),
                            Toolbar.component({ items: useScalarComponent }),
                            Toolbar.component({ items: scalarComponent }),
                            Toolbar.component({ items: minMaxComponent })];
                    } else {
                        this.useScalar(false);
                        this.component = [
                            Toolbar.component({ items: positionComponent }),
                            Toolbar.component({ items: useScalarComponent })];
                    }
                }
                return this.useScalar();
            }

            let positionComponent = [
                { component: "Position: " },
                { component: Choice.component({ model: positionModel, getOptions: getOptions, useDictValueForLabel: true }) },
            ];

            let useScalarComponent = [
                { component: "Use color scale: " },
                { component: Checkbox.component({ model: checkboxModel }) },
            ];

            let scalarComponent = [
                { component: "Scalar: " },
                { component: Choice.component({ model: scalarModel, getOptions: getOptions, useDictValueForLabel: true }) },
            ];

            let minMaxComponent = [
                { component: "Min: " },
                {
                    component: Spinner.component({
                        model: this.min,
                        increment: 1.0,
                        showLabel: false,
                        decimal: 0,
                    })
                },
                { component: "Desired: " },
                {
                    component: Spinner.component({
                        model: this.desired,
                        increment: 1.0,
                        min: 0,
                        showLabel: false,
                        decimal: 0,
                    })
                },
                { component: "Max: " },
                {
                    component: Spinner.component({
                        model: this.max,
                        increment: 1.0,
                        min: 0,
                        showLabel: false,
                        decimal: 0,
                    })
                }
            ];

            this.component = [
                Toolbar.component({ items: positionComponent }),
                Toolbar.component({ items: useScalarComponent })];
        }
    }

    document.title = 'Telemetry Viewer';
    return TelemetryViewer.mount($('.main-container')[0]);
});