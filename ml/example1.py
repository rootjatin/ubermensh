np.random.seed(0)

feature_maps = np.random.randn(64, 7, 7).astype(np.float32)
class_weights = np.random.randn(10, 64).astype(np.float32)

cam = compute_cam_advanced(
    feature_maps=feature_maps,
    class_weights=class_weights,
    class_idx=3,
    output_size=(224, 224),
    resize_mode="bilinear",
    activation="relu",
    normalize=True,
    clip_percentile=99.0,
    smooth=True,
    smooth_kernel=5,
    smooth_sigma=1.2
)

print(cam.shape)   # (224, 224)
print(cam.min(), cam.max())
