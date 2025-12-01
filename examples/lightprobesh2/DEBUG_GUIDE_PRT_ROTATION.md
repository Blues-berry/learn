# PRT Relighting Rotation Debug Guide

## Issue Summary
When PRT Relighting is enabled and the "Light" -> "Rotation" slider is dragged, the Cornell scene disappears.

## Root Causes Fixed

### 1. **Missing Vertex Buffer Binding**
**Problem**: The original code called `gltfModel->getModel()->draw(cmd)` directly without binding vertex buffers.

**Fix**: Added `gltfModel->getModel()->bindBuffers(cmd)` before drawing to ensure vertex and index buffers are properly bound.

### 2. **Missing Push Constants**
**Problem**: Material transformations and base colors were not being applied to the PRT pipeline.

**Fix**: Added proper push constant handling in the draw loop to apply model transformations and material colors.

### 3. **Incomplete Node Traversal**
**Problem**: The node tree wasn't being properly traversed to draw all primitives.

**Fix**: Implemented a recursive `drawNode` function that properly handles the scene hierarchy.

### 4. **Missing Validation**
**Problem**: No validation of rotation angle or SH coefficients.

**Fix**: Added comprehensive validation in `UpdatePRTLighting()` to check for NaN/Inf values and angle normalization.

## Debug Output Interpretation

### When Enabling PRT Relighting
Look for these debug messages:

```
[DEBUG PRT] Descriptor Set Details:
  - descriptorSetPRT: <non-null handle>
  - descriptorSetLayoutPRT: <non-null handle>
  - pipelineLayoutPRT: <non-null handle>
  - pipelinePRT: <non-null handle>
  - lightingSHBuffer.buffer: <non-null handle>
  - lightingSHBuffer.descriptor.range: 144  (should be sizeof(SHCoefficients))
```

**If any handle is null or range is 0**: Descriptor set initialization failed.

### When Rotating Light
Look for these debug messages:

```
[DEBUG] Light rotation changed to: 1.5708 rad (90 deg)
[DEBUG PRT] UpdatePRTLighting called:
  - Angle: 90 degrees (1.5708 radians)
  - prtData size: 24  (should be > 0)
  - lightingSHBuffer.mapped: YES
  - lightingSHBuffer.buffer: <non-null>
[DEBUG PRT] Updated Lighting UBO (Angle: 90 deg). L0M0 coeff: (0.718891, 0.718891, 0.718891)
```

**If prtData size is 0**: PRT data was not loaded. Check `LoadPRTData()`.
**If lightingSHBuffer.mapped is NO**: Buffer mapping failed.
**If L0M0 coefficients are 0 or NaN**: Coefficient interpolation failed.

### When Drawing with PRT Pipeline
Look for these debug messages (every 120 frames):

```
[DEBUG PRT] Binding PRT Pipeline. Pipeline Handle: <handle>
  , Layout: <handle>
  , Global Set: <handle>
  , PRT Set: <handle>
[DEBUG PRT] Light Rotation Angle: 1.5708 rad (90 deg)
```

**If any handle is null**: Pipeline or descriptor set is not properly initialized.

## Troubleshooting Steps

### Step 1: Verify PRT Data is Loaded
Check console for:
```
[DEBUG PRT] Loaded 2399 per-vertex LT coefficients from prt_output/prt_data_lt_batch.txt
[DEBUG PRT] First LT coefficient (l=0, m=0): (0.718891, 0.718891, 0.718891)
```

If not present:
- Ensure `prt_output/prt_data_lt_batch.txt` exists
- Run PRT precomputation: Enable "PRT GPU Export" and click "Precompute PRT"

### Step 2: Verify Descriptor Sets
Check console for descriptor set details. All handles should be non-null.

If descriptors are null:
- Check that `preparePRTRelightingResources()` was called
- Verify `preparePRTRelightingPipeline()` was called after resources

### Step 3: Verify Rotation Angle Updates
Drag the "Light" -> "Rotation" slider and check console for:
```
[DEBUG] Light rotation changed to: <angle>
```

If angle doesn't change:
- UI slider may not be connected properly
- Check `OnUpdateUIOverlay()` for slider binding

### Step 4: Verify Coefficient Updates
Check console for:
```
[DEBUG PRT] Updated Lighting UBO (Angle: <angle> deg). L0M0 coeff: (<r>, <g>, <b>)
```

If coefficients are all zeros or NaN:
- Check `Relighter::QueryCoefficients()` implementation
- Verify `prtData` is properly populated
- Check angle normalization (should be 0-360)

### Step 5: Verify Pipeline Binding
Check console for:
```
[DEBUG PRT] Binding PRT Pipeline. Pipeline Handle: <non-null>
```

If pipeline handle is null:
- `preparePRTRelightingPipeline()` may have failed
- Check shader compilation errors
- Verify descriptor set layout matches pipeline layout

## Key Variables to Monitor

1. **lightRotationAngle**: Should increase when slider is dragged
2. **usePRTRelighting**: Should be true when checkbox is enabled
3. **prtData**: Should contain 24 rotation sets of SH coefficients
4. **currentSHCoefficients**: Should update each frame with interpolated values
5. **descriptorSetPRT**: Should be non-null after pipeline preparation
6. **pipelinePRT**: Should be non-null after pipeline creation

## Expected Behavior

1. **Enable PRT Relighting**: Scene should remain visible
2. **Drag Light Rotation slider**: Scene should update lighting in real-time
3. **Rotation angle 0°**: Should use original lighting
4. **Rotation angle 180°**: Should use opposite direction lighting
5. **Rotation angle 360°**: Should wrap back to 0° (same as 0°)

## If Scene Still Disappears

1. Check for Vulkan validation errors in console
2. Verify all descriptor sets are properly bound
3. Check that vertex buffers are bound before drawing
4. Verify push constants are correctly sized
5. Check that pipeline layout matches descriptor set layouts
6. Ensure render pass is compatible with pipeline

## Performance Notes

- Debug logging occurs every 120 frames to avoid console spam
- Angle change logging only triggers when angle changes > 0.01 radians
- Invalid coefficient detection is done every frame but only logged on error

