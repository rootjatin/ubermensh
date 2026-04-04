import numpy as np


def relu(x):
    return np.maximum(0, x)


def _validate_feature_maps(feature_maps):
    feature_maps = np.asarray(feature_maps, dtype=np.float32)

    if feature_maps.ndim == 3:
        # (C, H, W) -> (1, C, H, W)
        feature_maps = feature_maps[None, ...]
    elif feature_maps.ndim != 4:
        raise ValueError("feature_maps must have shape (C,H,W) or (N,C,H,W)")

    return feature_maps


def _validate_class_weights(class_weights):
    class_weights = np.asarray(class_weights, dtype=np.float32)
    if class_weights.ndim != 2:
        raise ValueError("class_weights must have shape (num_classes, C)")
    return class_weights


def _resolve_class_indices(class_idx, num_classes):
    if class_idx is None:
        return np.arange(num_classes)

    if isinstance(class_idx, (int, np.integer)):
        if not (0 <= class_idx < num_classes):
            raise ValueError("class_idx out of range")
        return np.array([class_idx], dtype=np.int64)

    class_idx = np.asarray(class_idx, dtype=np.int64)
    if class_idx.ndim != 1:
        raise ValueError("class_idx must be int, list, tuple, ndarray, or None")

    if np.any(class_idx < 0) or np.any(class_idx >= num_classes):
        raise ValueError("Some class indices are out of range")

    return class_idx


def _apply_activation(cam, mode="relu"):
    if mode == "relu":
        return relu(cam)
    if mode == "abs":
        return np.abs(cam)
    if mode == "none":
        return cam
    raise ValueError("activation must be one of: 'relu', 'abs', 'none'")


def _normalize_map(cam, eps=1e-12, clip_percentile=None):
    """
    Normalize per map to [0, 1].
    cam shape: (..., H, W)
    """
    cam = cam.astype(np.float32)

    if clip_percentile is not None:
        if not (0 < clip_percentile <= 100):
            raise ValueError("clip_percentile must be in (0, 100]")
        hi = np.percentile(cam, clip_percentile, axis=(-2, -1), keepdims=True)
        lo = np.percentile(cam, 100 - clip_percentile, axis=(-2, -1), keepdims=True)
        cam = np.clip(cam, lo, hi)

    cam_min = np.min(cam, axis=(-2, -1), keepdims=True)
    cam_max = np.max(cam, axis=(-2, -1), keepdims=True)

    denom = np.maximum(cam_max - cam_min, eps)
    cam = (cam - cam_min) / denom

    # if map is constant, force zeros
    constant_mask = (cam_max - cam_min) < eps
    cam = np.where(constant_mask, 0.0, cam)

    return cam.astype(np.float32)


def resize_nearest(image, out_h, out_w):
    """
    image: (H, W)
    """
    in_h, in_w = image.shape
    row_idx = (np.arange(out_h) * in_h / out_h).astype(int)
    col_idx = (np.arange(out_w) * in_w / out_w).astype(int)

    row_idx = np.clip(row_idx, 0, in_h - 1)
    col_idx = np.clip(col_idx, 0, in_w - 1)

    return image[row_idx[:, None], col_idx[None, :]]


def resize_bilinear(image, out_h, out_w):
    """
    image: (H, W)
    Pure NumPy bilinear resize.
    """
    in_h, in_w = image.shape

    if out_h == 1:
        y = np.array([0.0], dtype=np.float32)
    else:
        y = np.linspace(0, in_h - 1, out_h, dtype=np.float32)

    if out_w == 1:
        x = np.array([0.0], dtype=np.float32)
    else:
        x = np.linspace(0, in_w - 1, out_w, dtype=np.float32)

    y0 = np.floor(y).astype(np.int32)
    x0 = np.floor(x).astype(np.int32)
    y1 = np.clip(y0 + 1, 0, in_h - 1)
    x1 = np.clip(x0 + 1, 0, in_w - 1)

    wy = y - y0
    wx = x - x0

    Ia = image[y0[:, None], x0[None, :]]
    Ib = image[y1[:, None], x0[None, :]]
    Ic = image[y0[:, None], x1[None, :]]
    Id = image[y1[:, None], x1[None, :]]

    wa = (1 - wy)[:, None] * (1 - wx)[None, :]
    wb = wy[:, None] * (1 - wx)[None, :]
    wc = (1 - wy)[:, None] * wx[None, :]
    wd = wy[:, None] * wx[None, :]

    out = Ia * wa + Ib * wb + Ic * wc + Id * wd
    return out.astype(np.float32)


def _resize_maps(cams, output_size, mode="bilinear"):
    """
    cams: (N, K, H, W)
    """
    out_h, out_w = output_size
    N, K, _, _ = cams.shape
    out = np.empty((N, K, out_h, out_w), dtype=np.float32)

    resize_fn = resize_bilinear if mode == "bilinear" else resize_nearest

    for n in range(N):
        for k in range(K):
            out[n, k] = resize_fn(cams[n, k], out_h, out_w)

    return out


def gaussian_kernel1d(kernel_size=5, sigma=1.0):
    if kernel_size % 2 == 0 or kernel_size < 1:
        raise ValueError("kernel_size must be a positive odd integer")

    radius = kernel_size // 2
    x = np.arange(-radius, radius + 1, dtype=np.float32)
    kernel = np.exp(-(x ** 2) / (2 * sigma ** 2))
    kernel /= np.sum(kernel)
    return kernel.astype(np.float32)


def _convolve_along_axis(images, kernel, axis):
    """
    images: (..., H, W)
    axis: -2 or -1
    """
    pad = len(kernel) // 2
    pad_width = [(0, 0)] * images.ndim
    pad_width[axis] = (pad, pad)

    padded = np.pad(images, pad_width, mode="reflect")
    out = np.zeros_like(images, dtype=np.float32)

    for i, w in enumerate(kernel):
        sl = [slice(None)] * images.ndim
        sl[axis] = slice(i, i + images.shape[axis])
        out += w * padded[tuple(sl)]

    return out


def gaussian_blur(maps, kernel_size=5, sigma=1.0):
    """
    maps: (..., H, W)
    """
    kernel = gaussian_kernel1d(kernel_size, sigma)
    out = _convolve_along_axis(maps, kernel, axis=-2)
    out = _convolve_along_axis(out, kernel, axis=-1)
    return out.astype(np.float32)


def global_average_pool(feature_maps):
    """
    feature_maps: (N, C, H, W)
    returns: (N, C)
    """
    return np.mean(feature_maps, axis=(-2, -1))


def predict_scores_from_gap(feature_maps, class_weights, bias=None):
    """
    feature_maps: (N, C, H, W)
    class_weights: (num_classes, C)
    bias: (num_classes,) or None

    returns:
        scores: (N, num_classes)
    """
    gap = global_average_pool(feature_maps)                # (N, C)
    scores = gap @ class_weights.T                         # (N, K)
    if bias is not None:
        bias = np.asarray(bias, dtype=np.float32)
        if bias.shape != (class_weights.shape[0],):
            raise ValueError("bias must have shape (num_classes,)")
        scores = scores + bias
    return scores.astype(np.float32)


def compute_cam_advanced(
    feature_maps,
    class_weights,
    class_idx=None,
    output_size=None,
    resize_mode="bilinear",
    activation="relu",
    normalize=True,
    clip_percentile=None,
    smooth=False,
    smooth_kernel=5,
    smooth_sigma=1.0,
    return_scores=False,
    bias=None,
):
    """
    Advanced CAM using only NumPy.

    Parameters
    ----------
    feature_maps : np.ndarray
        Shape (C,H,W) or (N,C,H,W)
    class_weights : np.ndarray
        Shape (num_classes, C)
    class_idx : int, sequence, or None
        - int: one class
        - list/tuple/ndarray: multiple classes
        - None: all classes
    output_size : tuple or None
        (out_h, out_w)
    resize_mode : str
        'nearest' or 'bilinear'
    activation : str
        'relu', 'abs', or 'none'
    normalize : bool
        Normalize each CAM into [0, 1]
    clip_percentile : float or None
        Example: 99.0 for robust normalization
    smooth : bool
        Apply Gaussian smoothing
    smooth_kernel : int
        Odd kernel size
    smooth_sigma : float
        Gaussian sigma
    return_scores : bool
        Return class scores estimated by GAP + FC
    bias : np.ndarray or None
        Final FC bias, shape (num_classes,)

    Returns
    -------
    cams : np.ndarray
        If input was (C,H,W) and one class -> (H,W)
        If input was (C,H,W) and many classes -> (K,H,W)
        If input was (N,C,H,W) and one class -> (N,H,W)
        If input was (N,C,H,W) and many classes -> (N,K,H,W)

    Optional extra if return_scores=True:
        (cams, scores, selected_classes)
    """
    feature_maps = _validate_feature_maps(feature_maps)      # (N,C,H,W)
    class_weights = _validate_class_weights(class_weights)   # (K,C)

    N, C, H, W = feature_maps.shape
    num_classes, weight_channels = class_weights.shape

    if C != weight_channels:
        raise ValueError(
            f"Channel mismatch: feature_maps has {C} channels, "
            f"class_weights expects {weight_channels}"
        )

    selected_classes = _resolve_class_indices(class_idx, num_classes)  # (M,)
    selected_weights = class_weights[selected_classes]                 # (M,C)

    # CAM = sum_c w_kc * f_c
    # feature_maps: (N,C,H,W)
    # selected_weights: (M,C)
    # result: (N,M,H,W)
    cams = np.einsum("mc,nchw->nmhw", selected_weights, feature_maps).astype(np.float32)

    cams = _apply_activation(cams, activation)

    if smooth:
        cams = gaussian_blur(cams, kernel_size=smooth_kernel, sigma=smooth_sigma)

    if output_size is not None:
        if resize_mode not in ("nearest", "bilinear"):
            raise ValueError("resize_mode must be 'nearest' or 'bilinear'")
        cams = _resize_maps(cams, output_size, mode=resize_mode)

    if normalize:
        cams = _normalize_map(cams, clip_percentile=clip_percentile)

    # Squeeze smartly for convenience
    single_input = (np.asarray(feature_maps).shape[0] == 1) if feature_maps.ndim == 4 else False
    one_class = len(selected_classes) == 1

    if N == 1 and one_class:
        cams_out = cams[0, 0]
    elif N == 1:
        cams_out = cams[0]
    elif one_class:
        cams_out = cams[:, 0]
    else:
        cams_out = cams

    if return_scores:
        scores = predict_scores_from_gap(feature_maps, class_weights, bias=bias)
        if N == 1:
            scores = scores[0]
        return cams_out, scores, selected_classes

    return cams_out
