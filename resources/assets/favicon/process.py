from PIL import Image
import os

def generate_favicons(input_image_path, output_dir="output"):
    """生成多种分辨率的PNG图标并创建ICO文件"""
    
    # 创建输出目录
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # 定义目标分辨率
    sizes = [24, 32, 48, 64, 128, 256, 512, 1024]
    
    # 打开原始图片
    try:
        original = Image.open(input_image_path)
    except Exception as e:
        print(f"无法打开图片: {e}")
        return
    
    # 转换为 RGBA 模式（支持透明通道）
    if original.mode != 'RGBA':
        original = original.convert('RGBA')
    
    png_files = []
    
    # 生成各个分辨率的 PNG
    for size in sizes:
        # 使用高质量重采样算法缩放图片
        resized = original.resize((size, size), Image.Resampling.LANCZOS)
        
        # 保存为 PNG
        filename = f"favicon_{size}.png"
        filepath = os.path.join(output_dir, filename)
        resized.save(filepath, "PNG", optimize=True)
        png_files.append(filepath)
        print(f"已生成: {filename}")
    
    # 创建 ICO 文件（Windows 图标）
    # ICO 文件可以包含多个尺寸，但通常最大到 256x256
    ico_sizes = [16, 24, 32, 48, 64, 128, 256]
    ico_images = []
    
    for size in ico_sizes:
        if size <= max(sizes):
            resized = original.resize((size, size), Image.Resampling.LANCZOS)
            ico_images.append(resized)
    
    # 保存 ICO 文件
    ico_path = os.path.join(output_dir, "favicon.ico")
    ico_images[0].save(
        ico_path,
        format="ICO",
        sizes=[(img.width, img.height) for img in ico_images],
        append_images=ico_images[1:]
    )
    print(f"\n已生成 ICO 文件: favicon.ico")
    print(f"ICO 包含尺寸: {[img.size for img in ico_images]}")
    
    return png_files

if __name__ == "__main__":
    # 使用示例 - 请修改为你的图片路径
    input_image = "favicon_2048.png"  # 替换为你的图片路径
    
    generate_favicons(input_image)