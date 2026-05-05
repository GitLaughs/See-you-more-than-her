import argparse
import math
import random
from pathlib import Path

import cv2


VIDEO_EXTENSIONS = {".mp4", ".avi", ".mov", ".mkv", ".mpg", ".mpeg"}
DEFAULT_SOURCE_DIR = "/mnt/hdd16t0/dataset/rps_dataset/datasets"
DEFAULT_OUTPUT_DIR = "/mnt/hdd16t0/dataset/rps_dataset/processed_dataset/N"
DEFAULT_TARGET_CROP = (210, 270, 750, 810)  # x1, y1, x2, y2


def iter_video_files(dataset_dir: Path) -> list[Path]:
    videos = []
    for path in sorted(dataset_dir.rglob("*")):
        if path.is_file() and path.suffix.lower() in VIDEO_EXTENSIONS:
            videos.append(path)
    return videos


def parse_crop(values: list[int]) -> tuple[int, int, int, int]:
    if len(values) != 4:
        raise ValueError("crop must contain exactly 4 integers: x1 y1 x2 y2")
    x1, y1, x2, y2 = values
    if x1 < 0 or y1 < 0 or x2 <= x1 or y2 <= y1:
        raise ValueError("invalid crop range, expected x2 > x1 and y2 > y1")
    return x1, y1, x2, y2


def sample_frame_indices(
    frame_count: int, samples_per_video: int, rng: random.Random
) -> list[int]:
    sample_count = min(frame_count, samples_per_video)
    indices = []

    for i in range(sample_count):
        start = math.floor(i * frame_count / sample_count)
        end = math.floor((i + 1) * frame_count / sample_count) - 1
        end = max(start, end)
        indices.append(rng.randint(start, end))

    return sorted(set(indices))


def rectangles_overlap(
    rect_a: tuple[int, int, int, int], rect_b: tuple[int, int, int, int]
) -> bool:
    ax1, ay1, ax2, ay2 = rect_a
    bx1, by1, bx2, by2 = rect_b
    return not (ax2 <= bx1 or bx2 <= ax1 or ay2 <= by1 or by2 <= ay1)


def sample_negative_crop(
    frame_width: int,
    frame_height: int,
    crop_width: int,
    crop_height: int,
    target_crop: tuple[int, int, int, int],
    rng: random.Random,
    max_attempts: int = 200,
) -> tuple[int, int, int, int]:
    max_x = frame_width - crop_width
    max_y = frame_height - crop_height
    if max_x < 0 or max_y < 0:
        raise RuntimeError(
            f"crop size {(crop_width, crop_height)} exceeds frame size {(frame_width, frame_height)}"
        )

    for _ in range(max_attempts):
        x1 = rng.randint(0, max_x)
        y1 = rng.randint(0, max_y)
        candidate = (x1, y1, x1 + crop_width, y1 + crop_height)
        if not rectangles_overlap(candidate, target_crop):
            return candidate

    step = 10
    candidates = []
    for x1 in range(0, max_x + 1, step):
        for y1 in range(0, max_y + 1, step):
            candidate = (x1, y1, x1 + crop_width, y1 + crop_height)
            if not rectangles_overlap(candidate, target_crop):
                candidates.append(candidate)

    if not candidates:
        raise RuntimeError(
            "no valid negative crop found that does not overlap the target crop"
        )

    return rng.choice(candidates)


def read_frame_with_fallback(
    capture: cv2.VideoCapture,
    frame_index: int,
    frame_count: int,
    search_radius: int = 5,
):
    candidate_indices = [frame_index]
    for offset in range(1, search_radius + 1):
        lower = frame_index - offset
        upper = frame_index + offset
        if lower >= 0:
            candidate_indices.append(lower)
        if upper < frame_count:
            candidate_indices.append(upper)

    for candidate_index in candidate_indices:
        capture.set(cv2.CAP_PROP_POS_FRAMES, candidate_index)
        ok, frame = capture.read()
        if ok and frame is not None:
            return candidate_index, frame

    raise RuntimeError(
        f"failed to read frame {frame_index} and nearby frames within radius {search_radius}"
    )


def process_video(
    video_path: Path,
    output_dir: Path,
    target_crop: tuple[int, int, int, int],
    samples_per_video: int,
    image_ext: str,
    rng: random.Random,
) -> int:
    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"failed to open video: {video_path}")

    frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
    frame_width = int(capture.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_height = int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
    if frame_count <= 0:
        capture.release()
        raise RuntimeError(f"failed to read frame count: {video_path}")

    tx1, ty1, tx2, ty2 = target_crop
    crop_width = tx2 - tx1
    crop_height = ty2 - ty1

    frame_indices = sample_frame_indices(frame_count, samples_per_video, rng)
    saved_count = 0

    for frame_index in frame_indices:
        actual_frame_index, frame = read_frame_with_fallback(
            capture=capture,
            frame_index=frame_index,
            frame_count=frame_count,
        )

        crop_rect = sample_negative_crop(
            frame_width=frame_width,
            frame_height=frame_height,
            crop_width=crop_width,
            crop_height=crop_height,
            target_crop=target_crop,
            rng=rng,
        )
        x1, y1, x2, y2 = crop_rect
        cropped = frame[y1:y2, x1:x2]

        output_name = (
            f"{video_path.stem}_frame_{actual_frame_index:06d}_neg_x{x1}_y{y1}{image_ext}"
        )
        output_path = output_dir / output_name
        if not cv2.imwrite(str(output_path), cropped):
            raise RuntimeError(f"failed to save frame: {output_path}")

        saved_count += 1

    capture.release()
    return saved_count


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Sample negative crops from videos and save them as class N"
    )
    parser.add_argument(
        "--dataset_dir",
        type=str,
        default=DEFAULT_SOURCE_DIR,
        help="Input video dataset root directory",
    )
    parser.add_argument(
        "--output_dir",
        type=str,
        default=DEFAULT_OUTPUT_DIR,
        help="Output directory for negative class N images",
    )
    parser.add_argument(
        "--target_crop",
        nargs=4,
        type=int,
        default=DEFAULT_TARGET_CROP,
        metavar=("X1", "Y1", "X2", "Y2"),
        help="Target crop region to avoid: x1 y1 x2 y2",
    )
    parser.add_argument(
        "--samples_per_video",
        type=int,
        default=100,
        help="Number of frames to sample per video",
    )
    parser.add_argument(
        "--image_ext",
        type=str,
        default=".png",
        choices=[".png", ".jpg", ".jpeg", ".bmp"],
        help="Image extension for saved frames",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for reproducibility",
    )
    args = parser.parse_args()

    dataset_dir = Path(args.dataset_dir)
    output_dir = Path(args.output_dir)
    target_crop = parse_crop(list(args.target_crop))

    if not dataset_dir.exists():
        raise RuntimeError(f"dataset directory not found: {dataset_dir}")

    videos = iter_video_files(dataset_dir)
    if not videos:
        raise RuntimeError(f"no video files found under: {dataset_dir}")

    output_dir.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)

    print(f"Found {len(videos)} videos under {dataset_dir}")
    print(f"Target crop to avoid : {target_crop}")
    print(f"Samples per video    : {args.samples_per_video}")
    print(f"Output dir           : {output_dir}")

    total_saved = 0
    for index, video_path in enumerate(videos, start=1):
        saved_count = process_video(
            video_path=video_path,
            output_dir=output_dir,
            target_crop=target_crop,
            samples_per_video=args.samples_per_video,
            image_ext=args.image_ext,
            rng=rng,
        )
        total_saved += saved_count
        print(
            f"[{index}/{len(videos)}] {video_path.name} -> N, saved {saved_count} frames"
        )

    print(f"Finished. Total saved frames: {total_saved}")


if __name__ == "__main__":
    main()
