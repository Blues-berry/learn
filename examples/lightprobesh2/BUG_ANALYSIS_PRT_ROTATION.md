# PRT Relighting Scene Disappearance Bug Analysis

## Problem Statement
When PRT Relighting is enabled and the "Light" -> "Rotation" slider is dragged, the Cornell scene disappears from the viewport.

## Root Cause Analysis

### Issue 1: Descriptor Set Binding Mismatch
**Location**: `main.cpp`, line 718-719 in `drawFrame()`

```cpp
std::array<VkDescriptorSet, 2> prtDescriptorSets = { mainPass->descriptorSet, descriptorSetPRT };
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutPRT, 0, 2, prtDescriptorSets.data(), 0, nullptr);
```

**Problem**: The PRT pipeline layout (`pipelineLayoutPRT`) expects descriptor sets at specific binding points. However:
1. The descriptor set layout for the PRT pipeline may not match the layout of `mainPass->descriptorSet`
2. The binding indices (0, 1) may not correspond to the actual layout bindings defined in the PRT pipeline creation
3. When the light rotation slider changes, `UpdatePRTLighting()` updates the `lightingSHBuffer` UBO, but the descriptor set binding may be invalid

### Issue 2: Missing Vertex Input State
**Location**: `main.cpp`, line 721 in `drawFrame()`

```cpp
gltfModel->getModel()->draw(cmd);
```

**Problem**: The low-level `draw()` function assumes:
1. Vertex buffers are already bound
2. Index buffers are already bound
3. The pipeline has compatible vertex input state

However, when switching from the normal PBR pipeline to the PRT pipeline:
- The vertex input state may differ
- The vertex buffer bindings may not be set up correctly for the PRT pipeline
- This causes the geometry to not render properly

### Issue 3: Push Constants Not Applied
**Location**: `main.cpp`, line 721 in `drawFrame()`

**Problem**: The normal `gltfModel->Draw()` function applies:
1. Material data (albedo, roughness, metallic)
2. Push constants for model transformation
3. Descriptor set bindings for materials

When calling `gltfModel->getModel()->draw(cmd)` directly, these are bypassed, causing:
- Missing material information
- Missing model transformations
- Incorrect rendering state

### Issue 4: Incomplete PRT Pipeline Setup
**Location**: `main.cpp`, lines 1680-1720 in `PreparePRTPipeline()`

**Problem**: The PRT pipeline creation may not properly define:
1. All required descriptor set layouts
2. Correct binding indices for the lighting SH buffer
3. Proper vertex input state matching the glTF model format

## Debug Points to Add

1. **Verify descriptor set validity**
   - Check if `descriptorSetPRT` is properly initialized
   - Verify descriptor set layout matches pipeline layout

2. **Check buffer updates**
   - Confirm `lightingSHBuffer` is being updated correctly
   - Verify buffer memory is properly mapped

3. **Validate pipeline state**
   - Ensure PRT pipeline has correct vertex input state
   - Verify all descriptor set bindings are correct

4. **Monitor rotation angle**
   - Log the rotation angle value when slider changes
   - Verify angle is correctly converted to degrees
   - Check if angle is within valid range for coefficient interpolation

## Recommended Fixes

### Fix 1: Use GltfModel::Draw with PRT Pipeline Override
Instead of calling `gltfModel->getModel()->draw(cmd)` directly, pass the PRT pipeline as an override:

```cpp
gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, pipelinePRT);
```

This ensures:
- Material data is properly applied
- Push constants are set correctly
- Vertex buffers are properly bound

### Fix 2: Verify Descriptor Set Binding
Ensure the descriptor set layout for PRT matches the expected bindings:

```cpp
// In PreparePRTPipeline():
// Verify descriptorSetLayoutPRT has correct bindings
// Ensure descriptorSetPRT is created with matching layout
```

### Fix 3: Add Comprehensive Debug Logging
Add debug output to track:
- Rotation angle changes
- Descriptor set updates
- Buffer memory mapping status
- Pipeline binding state

### Fix 4: Validate Coefficient Interpolation
Ensure `Relighter::QueryCoefficients()` returns valid data:
- Check angle range (0-360 degrees)
- Verify prtData is not empty
- Validate returned coefficients are not NaN/Inf

