"""Independent OV5640 RAW test-pattern oracle for the configured guard crop."""
import numpy as np

COLORS = np.array([[1, 1, 1], [1, 1, 0], [0, 1, 1], [0, 1, 0],
                   [1, 0, 1], [1, 0, 0], [0, 0, 1], [0, 0, 0]], dtype=float)


def raw_guard_bars(image, fmt, tolerance=50):
    """Predict bars before the 16-pixel horizontal guard crop, never fit them.

    geometry_base configures x_start=0, x_end=width+31 and x_offset=16.
    The RAW tap crops this window rather than ISP-scaling bars to output width.
    Boundary neighborhoods are excluded to allow integer generator divisions.
    """
    height, width = image.shape
    position = (np.arange(width, dtype=float)+16)*8/(width+32)
    band = np.minimum(position.astype(int), 7)
    interior = (position % 1 > .18) & (position % 1 < .82)
    expected_rgb = COLORS[band]*255
    scale = 255 if fmt == 'BayerBG8' else 1023
    errors = []
    for dy, dx, channel in [(0, 0, 2), (0, 1, 1), (1, 0, 1), (1, 1, 0)]:
        actual = image[dy::2, dx::2].astype(float)*255/scale
        expected = expected_rgb[dx::2, channel]
        keep = interior[dx::2]
        assert keep.sum() >= 4, 'Insufficient RAW bar interiors'
        delta = np.abs(actual[height//16:7*height//16, keep]-expected[keep])
        errors.append(float(delta.max()))
    assert max(errors) <= tolerance, f'RAW guard-crop oracle failed: phase errors {errors}'
    return dict(oracle='RAW sensor-window bars with 16-pixel guard crop',
                input_window_width=width+32, horizontal_offset=16, bayer_phase='BGGR',
                maximum_phase_errors=errors, tolerance=tolerance)
