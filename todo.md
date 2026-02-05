# TODO
-- Факел, код тумана:
if (fogStrength > 0.0f)
{
    float px = player.GetX();
    float py = player.GetY();
    float playerScreenX = (px - camera.x) * camera.zoom;
    float playerScreenY = (py - camera.y) * camera.zoom;
    float fogRadiusScreen = fogRadius * camera.zoom;
    const int screenW = renderer->width;
    const int screenH = renderer->height;
    const float step = 32.0f;
    for (float y = 0; y < screenH; y += step)
    {
        for (float x = 0; x < screenW; x += step)
        {
            float centerX = x + step * 0.5f;
            float centerY = y + step * 0.5f;
            float dx = centerX - playerScreenX;
            float dy = centerY - playerScreenY;
            float dist = sqrtf(dx*dx + dy*dy);
            float alpha = std::min(1.0f, dist / fogRadiusScreen * fogStrength);
            Renderer_DrawRectangle(Rect{x, y, step, step}, Color{0,0,0,alpha});
        }
    }
}