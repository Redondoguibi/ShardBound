(() => {
    if (location.href !== "https://web.blockbench.net/" || window !== window.top) return;

    const events = window.chrome.webview;
    let project;
    let saving = false;
    const delay = ms => new Promise(resolve => setTimeout(resolve, ms));

    function starter(name) {
        setupProject(Formats.free);
        Project.name = name;
        Project.box_uv = false;
        Project.texture_width = 64;
        Project.texture_height = 32;
        const canvas = document.createElement("canvas");
        canvas.width = 64;
        canvas.height = 32;
        const ctx = canvas.getContext("2d");
        // Each face has its own 16 × 16 tile so paint does not repeat on other faces.
        for (let y = 0; y < 32; y++) {
            for (let x = 0; x < 48; x++) {
                const tile = Math.floor(x / 16) + 3 * Math.floor(y / 16);
                ctx.fillStyle = ["#668fa1", "#63899b", "#63899b", "#608497", "#789eae", "#4e6c80"][tile];
                ctx.fillRect(x, y, 1, 1);
            }
        }
        const texture = new Texture({name: "Bloco.png", uv_width: 64, uv_height: 32})
            .fromDataURL(canvas.toDataURL()).add();
        const names = ["north", "east", "south", "west", "up", "down"];
        const faces = Object.fromEntries(names.map((face, i) => {
            const x = (i % 3) * 16;
            const y = Math.floor(i / 3) * 16;
            return [face, {uv: [x, y, x + 16, y + 16], texture: texture.uuid}];
        }));
        new Cube({name: "Bloco", from: [-8, 0, -8], to: [8, 16, 8], faces}).init().select();
    }

    function encode(buffer) {
        const bytes = new Uint8Array(buffer);
        let data = "";
        for (let i = 0; i < bytes.length; i += 8192) {
            data += String.fromCharCode(...bytes.subarray(i, i + 8192));
        }
        return btoa(data);
    }

    async function save(ticket, name) {
        if (saving) return;
        saving = true;
        try {
            if (!project || Project !== project) throw new Error("Selecione o projeto do bloco nesta aba.");
            if (!Outliner.elements.some(element => element.export !== false && (element instanceof Cube || element instanceof Mesh))) {
                throw new Error("Crie pelo menos um cubo ou uma malha visível para exportação.");
            }
            const source = Codecs.project.compile({
                raw: true, backup: true, bitmaps: true, absolute_paths: false, reference_images: false
            });
            source.name = name;
            const glb = await Codecs.gltf.compile({
                encoding: "binary", scale: 16, embed_textures: true, armature: false,
                merge_armature: false, animations: false
            });
            if (!(glb instanceof ArrayBuffer) || glb.byteLength > 32 * 1024 * 1024) {
                throw new Error("O modelo ultrapassa o limite de 32 MiB.");
            }
            const json = JSON.stringify(source);
            if (new TextEncoder().encode(json).length > 16 * 1024 * 1024) {
                throw new Error("O projeto ultrapassa o limite de 16 MiB.");
            }
            const thumbnail = document.createElement("canvas");
            thumbnail.width = thumbnail.height = 16;
            const ctx = thumbnail.getContext("2d", {willReadFrequently: true});
            ctx.fillStyle = "#668fa1";
            ctx.fillRect(0, 0, 16, 16);
            const texture = project.textures.find(texture => texture.width && texture.height);
            if (texture) {
                ctx.imageSmoothingEnabled = false;
                ctx.drawImage(texture.canvas, 0, 0, 16, 16);
            }
            const rgba = ctx.getImageData(0, 0, 16, 16).data;
            const pixels = Array.from({length: 256}, (_, i) => (
                (rgba[i * 4] << 16) | (rgba[i * 4 + 1] << 8) | rgba[i * 4 + 2]
            ));
            events.postMessage({kind: "publish", ticket, source: json, glb: encode(glb), pixels});
        } catch (error) {
            events.postMessage({kind: "export_error", ticket, message: String(error.message || error)});
        } finally {
            saving = false;
        }
    }

    window.ShardBoundBlock = {
        async open(name, source) {
            try {
                for (let attempt = 0; attempt < 160; attempt++) {
                    if (window.Formats?.free && window.Codecs?.project && window.Codecs?.gltf &&
                        window.BarItems?.save_project && window.Blockbench?.setup_successful && window.Preview?.selected) break;
                    if (attempt === 159) throw new Error("O Blockbench não terminou de carregar.");
                    await delay(200);
                }
                if (source) Codecs.project.load(source, {path: "bloco.bbmodel", no_file: true});
                else starter(name);
                project = Project;
                if (!project || project.format.id !== "free") throw new Error("O editor não abriu o formato de bloco.");
                project.name = name;
                // Wait for embedded textures and their paint canvases before allowing export.
                await Promise.all(project.textures.map(texture => texture.img.decode()));
                const action = BarItems.save_project;
                const original = action.click;
                action.click = function (...args) {
                    if (Project === project) events.postMessage({kind: "save"});
                    else return original.apply(this, args);
                };
                document.addEventListener("keydown", event => {
                    if (Project === project && (event.ctrlKey || event.metaKey) &&
                        event.key.toLowerCase() === "s" && !event.shiftKey) {
                        event.preventDefault();
                        event.stopImmediatePropagation();
                        if (!event.repeat) events.postMessage({kind: "save"});
                    }
                }, true);
                const changed = () => {
                    if (Project === project) events.postMessage({kind: "changed"});
                };
                for (const event of ["finished_edit", "undo", "redo"]) Blockbench.on(event, changed);
                events.postMessage({kind: "ready"});
            } catch (error) {
                events.postMessage({kind: "setup_error", message: String(error.message || error)});
            }
        },
        save
    };
})();
