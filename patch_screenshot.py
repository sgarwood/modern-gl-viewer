import re
with open('src/application.cpp', 'r') as f: content = f.read()

# Add screenshot logic
logic = """
        static int frame_count = 0;
        if (++frame_count == 60) {
            extern void save_framebuffer_to_png(int, int, const std::string&);
            save_framebuffer_to_png(width, height, "/home/sgarwood/.gemini/antigravity-cli/brain/66790bf0-eae1-40b0-8b7f-3241d4cd3b8a/e2e_render.png");
            break;
        }
"""
if "save_framebuffer_to_png" not in content:
    content = content.replace("glfwSwapBuffers(impl_->window.get());", "glfwSwapBuffers(impl_->window.get());\n" + logic)
    with open('src/application.cpp', 'w') as f: f.write(content)
