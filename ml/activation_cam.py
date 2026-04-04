import numpy as np


def relu(x):
    return np.maximum(0, x)


def normalize_map(cam):
    cam = cam.astype(np.float32)
    cam_min = cam.min()
    cam_max = cam.max()

    if cam_max - cam_min < 1e-12:
        return np.zeros_like(cam, dtype=np.float32)

    return (cam - cam_min) / (cam_max - cam_min)


def resize_nearest(image, out_h, out_w):
    """
    Simple nearest-neighbor resize using only NumPy.
    image: (H, W)
    returns: (out_h, out_w)
    """
    in_h, in_w = image.shape

    row_idx = (np.arange(out_h) * in_h / out_h).astype(int)
    col_idx = (np.arange(out_w) * in_w / out_w).astype(int)

    row_idx = np.clip(row_idx, 0, in_h - 1)
    col_idx = np.clip(col_idx, 0, in_w - 1)

    return image[row_idx[:, None], col_idx[None, :]]


def compute_cam(feature_maps, class_weights, class_idx, output_size=None):
    """
    Compute Class Activation Map (CAM) using only NumPy.

    Parameters
    ----------
    feature_maps : np.ndarray
        Shape (C, H, W), output of last conv layer.
    class_weights : np.ndarray
        Shape (num_classes, C), weights of final FC layer.
    class_idx : int
        Target class index.
    output_size : tuple or None
        Optional (out_h, out_w) to resize CAM.

    Returns
    -------
    cam : np.ndarray
        Normalized CAM in range [0, 1].
    """
    if feature_maps.ndim != 3:
        raise ValueError("feature_maps must have shape (C, H, W)")

    if class_weights.ndim != 2:
        raise ValueError("class_weights must have shape (num_classes, C)")

    C, H, W = feature_maps.shape

    if class_weights.shape[1] != C:
        raise ValueError(
            f"Channel mismatch: feature_maps has {C} channels, "
            f"but class_weights expects {class_weights.shape[1]}"
        )

    if not (0 <= class_idx < class_weights.shape[0]):
        raise ValueError("class_idx out of range")

    # weights for selected class: shape (C,)
    weights = class_weights[class_idx]

    # Weighted sum across channels:
    # CAM(x, y) = sum_c weights[c] * feature_maps[c, x, y]
    cam = np.sum(feature_maps * weights[:, None, None], axis=0)

    # Standard CAM usually applies ReLU
    cam = relu(cam)

    # Normalize to [0, 1]
    cam = normalize_map(cam)

    # Optional resize
    if output_size is not None:
        out_h, out_w = output_size
        cam = resize_nearest(cam, out_h, out_w)
        cam = normalize_map(cam)

    return cam
