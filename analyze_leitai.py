import json
import os
import sys

def analyze_leitai_model():
    base_path = "c:/Users/Bluesky/Desktop/graphic/learn/assets/models/leitai"
    gltf_file = f"{base_path}/c7d96eefaa884afb839c1dd2d2fc4d41.gltf"
    
    print("=== Leitai Model Import Analysis ===\n")
    
    # 检查文件是否存在
    if not os.path.exists(gltf_file):
        print(f"❌ GLTF file not found: {gltf_file}")
        return False
    
    if not os.path.exists(f"{base_path}/buffer.bin"):
        print(f"❌ Buffer file not found: buffer.bin")
        return False
    
    print("✅ Required files exist:")
    print(f"  - GLTF: {os.path.getsize(gltf_file)} bytes")
    print(f"  - Buffer: {os.path.getsize(f'{base_path}/buffer.bin')} bytes")
    
    # 解析GLTF文件
    try:
        with open(gltf_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
        print("\n✅ GLTF file parsed successfully")
    except Exception as e:
        print(f"\n❌ Failed to parse GLTF: {e}")
        return False
    
    # 检查关键字段
    required_fields = ['buffers', 'bufferViews', 'meshes', 'nodes', 'materials']
    print("\n=== GLTF Structure ===")
    for field in required_fields:
        has_field = field in data
        count = len(data[field]) if has_field else 0
        status = "✅" if has_field else "❌"
        print(f"{status} {field}: {count}")
    
    # 检查纹理
    print("\n=== Texture Analysis ===")
    has_images = 'images' in data
    has_textures = 'textures' in data
    
    if has_images:
        print(f"✅ images: {len(data['images'])}")
        missing_textures = []
        
        for i, img in enumerate(data['images']):
            uri = img.get('uri', '')
            if uri:
                texture_path = f"{base_path}/{uri}"
                if os.path.exists(texture_path):
                    file_size = os.path.getsize(texture_path)
                    ext = os.path.splitext(uri)[1]
                    print(f"  ✅ [{ext}] {uri} ({file_size} bytes)")
                else:
                    print(f"  ❌ Missing: {uri}")
                    missing_textures.append(uri)
        
        if missing_textures:
            print(f"\n❌ {len(missing_textures)} missing textures!")
    else:
        print("❌ No images in GLTF")
    
    if has_textures:
        print(f"✅ textures: {len(data['textures'])}")
    
    # 检查materials
    print("\n=== Material Analysis ===")
    if 'materials' in data:
        materials = data['materials']
        print(f"Total materials: {len(materials)}")
        
        for i, mat in enumerate(materials[:5]):  # Show first 5
            name = mat.get('name', f'Material_{i}')
            has_base_color = 'pbrMetallicRoughness' in mat and 'baseColorTexture' in mat['pbrMetallicRoughness']
            print(f"  {i}: {name} - Has baseColorTexture: {has_base_color}")
    
    # 总结
    print("\n=== Import Verdict ===")
    
    # 检查是否可以使用LoadgltfModel
    issues = []
    
    if not has_images:
        issues.append("No images in GLTF (this is actually OK for basic import)")
    
    if missing_textures:
        issues.append(f"Missing {len(missing_textures)} texture files")
    
    if 'meshes' not in data or len(data['meshes']) == 0:
        issues.append("No meshes found")
    
    # 检查纹理格式
    jpg_count = 0
    png_count = 0
    if has_images:
        for img in data['images']:
            uri = img.get('uri', '')
            if uri.lower().endswith('.jpg') or uri.lower().endswith('.jpeg'):
                jpg_count += 1
            elif uri.lower().endswith('.png'):
                png_count += 1
    
    print(f"Texture formats: {jpg_count} JPG, {png_count} PNG")
    
    # vkglTF::Model应该支持JPG和PNG，因为tinygltf支持
    print("\n✅ vkglTF::Model (used by LoadgltfModel) should support:")
    print("  - glTF 2.0 format")
    print("  - Embedded buffer (.bin)")
    print("  - JPG/PNG textures (via stb_image)")
    
    if issues:
        print(f"\n⚠️  Potential issues:")
        for issue in issues:
            print(f"  - {issue}")
    else:
        print("\n✅ No major issues detected!")
    
    print("\n=== Recommendation ===")
    print("This model SHOULD be importable using LoadgltfModel!")
    print("The vkglTF::Model class uses tinygltf which supports:")
    print("  - Binary buffers (.bin)")
    print("  - JPG/PNG textures")
    print("  - glTF 2.0 specification")
    
    return True

if __name__ == "__main__":
    analyze_leitai_model()
