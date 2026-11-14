(() => {
    if (location.href !== "https://web.blockbench.net/" || window !== window.top) return;

    const events = window.chrome.webview;
    let project;
    let saving = false;
    let studio;
    let equipment;
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

    function equipmentStarter(name) {
        const model = studio.createProject(equipment.type, {id: equipment.id, name});
        Project.box_uv = false;
        const canvas = document.createElement("canvas");
        canvas.width = canvas.height = 64;
        const ctx = canvas.getContext("2d");
        const texture = new Texture({name: equipment.type === "armor" ? "Armadura.png" : "Arma.png", uv_width: 64, uv_height: 64});
        const slots = {head: ["Head"], chest: ["Body", "LeftArm", "RightArm"], legs: ["LeftLeg", "RightLeg"], feet: ["LeftLeg", "RightLeg"]};
        if (equipment.type === "armor") {
            const parts = slots[equipment.slot];
            for (const binding of model.bindings) {
                if (!parts.includes(binding.target)) studio.find(binding.group).remove();
            }
            model.bindings = model.bindings.filter(binding => parts.includes(binding.target));
            model.roots = model.bindings.map(binding => binding.group);
        }
        let piece = 0;
        function add(name, from, to, parent, colors) {
            const directions = ["north", "east", "south", "west", "up", "down"];
            const faces = Object.fromEntries(directions.map((direction, i) => {
                const tile = piece * 6 + i;
                const x = (tile % 8) * 8, y = Math.floor(tile / 8) * 8;
                ctx.fillStyle = colors[i % colors.length];
                ctx.fillRect(x, y, 8, 8);
                return [direction, {uv: [x, y, x + 8, y + 8], texture: texture.uuid}];
            }));
            ++piece;
            new Cube({name, from, to, origin: parent.origin.slice(), faces}).addTo(parent).init();
        }
        const steel = ["#879cac", "#647b8d", "#9db4c4"];
        const trim = ["#b59260", "#826540", "#cba879"];
        if (equipment.type === "armor") {
            for (const binding of model.bindings) {
                const group = studio.find(binding.group);
                const body = studio.find(binding.referenceGroup).children.find(child => child instanceof Cube);
                const from = body.from.map(value => value - 0.35);
                const to = body.to.map(value => value + 0.35);
                if (equipment.slot === "head") {
                    add("Coroa", [-4.5, 29, -4.5], [4.5, 32.5, 4.5], group, steel);
                    add("Proteção esquerda", [4, 25, -4.5], [4.5, 29, 4.5], group, trim);
                    add("Proteção direita", [-4.5, 25, -4.5], [-4, 29, 4.5], group, trim);
                } else {
                    if (equipment.slot === "feet") to[1] = 4;
                    if (equipment.slot === "legs") from[1] = 4;
                    if (equipment.slot === "chest" && binding.target !== "Body") from[1] = 20;
                    add(group.name.replace("Armadura_", ""), from, to, group, steel);
                }
            }
        } else {
            const grip = studio.find(model.roots[0]);
            add("Empunhadura", [-6, 14, -1], [-4, 19, 1], grip, trim);
            add("Guarda", [-9, 19, -1.2], [-1, 21, 1.2], grip, trim);
            add("Lâmina", [-6.5, 21, -.65], [-3.5, 35, .65], grip, steel);
        }
        texture.fromDataURL(canvas.toDataURL()).add();
        Project.texture_width = Project.texture_height = 64;
        studio.selectModel(model);
        studio.refresh();
    }

    function openClip(name, length, loop) {
        if (equipment?.type !== "melee" || Project !== project) return;
        try {
            const clip = Animation.all.find(entry => entry.name === name) || studio.createClip(name, length, loop);
            Modes.options.animate.select();
            clip.select();
        } catch (error) {
            events.postMessage({kind: "clip_error", message: String(error.message || error)});
        }
    }

    async function save(ticket, name, clips = []) {
        if (saving) return;
        saving = true;
        try {
            if (!project || Project !== project) throw new Error("Selecione o projeto desta aba no Blockbench.");
            if (studio) {
                if (equipment.type === "armor" && Animation.all.length) throw new Error("Armaduras acompanham a pose do jogador. Remova os clipes da peça antes de salvar.");
                if (equipment.type === "melee") {
                    const mode = Mode.selected;
                    const selected = Animation.selected;
                    for (const clip of clips) if (!Animation.all.some(value => value.name === clip.name)) {
                        studio.createClip(clip.name, clip.length, clip.loop);
                    }
                    mode?.select();
                    selected?.select();
                }
                const source = studio.codec.compile({raw: true, bitmaps: true, absolute_paths: false, reference_images: false});
                source.name = name;
                const compiled = await studio.exporter.compile();
                const json = JSON.stringify(source);
                if (new TextEncoder().encode(json).length > 16 * 1024 * 1024) throw new Error("O projeto ultrapassa 16 MiB.");
                const files = {};
                let size = 0;
                for (const [path, data] of compiled.files) {
                    size += typeof data === "string" ? new TextEncoder().encode(data).length : data.byteLength;
                    if (size > 32 * 1024 * 1024) throw new Error("O equipamento ultrapassa 32 MiB.");
                    files[path] = typeof data === "string" ? {text: data} : {base64: encode(data)};
                }
                events.postMessage({kind: "publish", ticket, source: json, files});
                return;
            }
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
        async open(name, source, options) {
            try {
                for (let attempt = 0; attempt < 160; attempt++) {
                    if (window.Formats?.free && window.Codecs?.project && window.Codecs?.gltf &&
                        window.BarItems?.save_project && window.Blockbench?.setup_successful && window.Preview?.selected) break;
                    if (attempt === 159) throw new Error("O Blockbench não terminou de carregar.");
                    await delay(200);
                }
                equipment = options;
                if (equipment) {
                    studio = window.createEmbeddedStudio();
                    if (equipment.type === "armor") studio.format.animation_mode = false;
                    if (source) studio.codec.load(source, {path: "equipamento.sbstudio", no_file: true});
                    else equipmentStarter(name);
                } else if (source) Codecs.project.load(source, {path: "bloco.bbmodel", no_file: true});
                else starter(name);
                project = Project;
                if (!project || project.format.id !== (equipment ? "shardbound_studio" : "free"))
                    throw new Error("O editor não abriu o formato do projeto.");
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
        save, openClip
    };
})();
