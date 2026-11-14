import { VERSION } from './core.mjs';
import { createStudio } from './controller.mjs';
import { createPreview } from './preview.mjs';
import { createExporter } from './exporter.mjs';
import { createUI } from './ui.mjs';
import { createMaterialPreview } from './materials.mjs';
let studio, ui;
BBPlugin.register('shardbound_studio', {
  title: 'ShardBound Studio', author: 'ShardBound', description: 'Autoria de modelos, skins, animações, partículas, VFX e sons sincronizados para ShardBound.',
  version: VERSION, min_version: '5.2.2', variant: 'desktop', tags: ['Animation', 'Model', 'Texture'],
  onload() {
    studio = createStudio(window); studio.preview = createPreview(window, studio); studio.exporter = createExporter(window, studio);
    studio.nativeRequire = requireNativeModule;
    studio.materialPreview = createMaterialPreview(window, studio);
    ui = createUI(window, studio, studio.exporter); studio.ui = ui;
    window.ShardBoundStudio = studio;
  },
  onunload() { ui?.dispose(); studio?.materialPreview?.dispose(); studio?.dispose(); delete window.ShardBoundStudio; ui = studio = null; },
});
