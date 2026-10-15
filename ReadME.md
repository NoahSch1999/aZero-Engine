# aZero Engine

### Description
*aZero Engine* is a game engine API with the focus on simplicity and flexibility.

It's a work-in-progress remake of my old game engine found here: https://github.com/NoahSch1999/aZeroEngine-Project

The idea is that the API provides easy-to-use and flexible engine-functionalities. Then the user can do whatever they want with it.

The API doesn't provide a build-in window-creation or input system since the idea is to make it as flexible as possible. Instead of having a window class, the API provides functionality to copy a rendered frame to your swapchain.

Right now this repository contains both the editor and engine, but the longterm plan is to split them into two seperate repositories.

### Structure
*Core classes*
 - Engine
	 - Main interface of the engine.
	 - Provides access to the other interfaces.

*Core api interfaces*
 - Scene
	 - Used to store and modify entities in your scene.
 - Renderer
	 - Interface for all render-related functionality such as rendering scenes, render-settings, and more...
 - RenderTarget
     - To be implemented
 - DepthStencilTarget
     - Interface that represents a depth and stencil target.
 - Swapchain
     - Interface that connects the rendered frames to the application window.

### Usage Guide
SETUP GUIDE FOR VISUAL STUDIO

Try installing the most up-to-date version of VS2022.

1. Clone repo

2. Right-click aZeroEditor folder and choose "Open with Visual Studio"

3. If the top-level directory doesn't have an "out" folder, CTRL+S the top-level CMakeLists.txt in Visual Studio

4. Build all with F7

5. Run with F5 

<!-- ### Implemented Features in this -->

### Features in the old engine that will be integrated in this
 - Fully gpu-driven rendering pipeline using compute shaders, mesh shaders and indirect drawing. Includes per-object and per-meshlet frustum culling.
 - Normal mapping
 - Shader hot-reloading and reflection that enables binding by shader resource name
 - Mesh, texture, and audio loading
 - Integrated audio engine
 - Entity-based scene configuration
 - Texture mip map and compression support
 - Integrated physics system with rigidbody and collider components

 - Dynamic amount of point, spot, and directional lights with frustrum culling for the first two
 - Deferred rendering (will probably be skipped for a more modern rendering technique)
 - Per-entity outlines (post process)
 - Color-based entity picking (optional)
 - Scene save/load
 - Material transparency maps
 - Entity parenting
 - Directional shadow mapping
 - Scene and material editor (incl. material drag/drop)
 - PBR shading
 - Toon shading
 
### Planned features:
 - Glow map materials using COD-style bloom
 - Clustered shading
 - Dedicated game and render thread
 - Cascading shadow maps
 - Omnidirectional shadow maps
 - Integrated scripting system using Mono
 - In-engine texture compression and mip generation
 - Mesh lod support
 - SMAA
 - Compute-based skeletal animation
 - Rendergraph
