import argparse
from pathlib import Path

import cv2


VIDEO_EXTENSIONS = {".mp4", ".avi", ".mov", ".mkv", ".mpg", ".mpeg"}
CLASS_NAMES = ["person", "stop", "forward", "obstacle", "NoTarget"]
FRAME_SIZE = (720, 1280)  # height, width
CENTER_CROP_SIZE = (320, 320)  # height, width
DEFAULT_CROP = (0, 0, 320, 320)  # x1, y1, x2, y2 on 320x320 center-cropped frame


def iter_video_files(dataset_dir: Path, class_name: str) -> list[Path]:
    class_dir = dataset_dir / class_name
    if not class_dir.exists():
        raise RuntimeError(f"missing class directory: {class_dir}")

    videos = []
    for path in sorted(class_dir.rglob("*")):
        if path.is_file() and path.suffix.lower() in VIDEO_EXTENSIONS:
            videos.append(path)
    if not videos:
        raise RuntimeError(f"no video files found under class directory: {class_dir}")
    return videos


def parse_crop(values: list[int]) -> tuple[int, int, int, int]:
    if len(values) != 4:
        raise ValueError("crop must contain exactly 4 integers: x1 y1 x2 y2")
    x1, y1, x2, y2 = values
    if x1 < 0 or y1 < 0 or x2 <= x1 or y2 <= y1:
        raise ValueError("invalid crop range, expected x2 > x1 and y2 > y1")
    return x1, y1, x2, y2


def center_crop(frame):
    height, width = frame.shape[:2]
    target_height, target_width = CENTER_CROP_SIZE
    if height < target_height or width < target_width:
        raise RuntimeError(
            f"frame size {(width, height)} is smaller than required {(FRAME_SIZE[1], FRAME_SIZE[0])}"
        )

    left = (width - target_width) // 2
    right = left + target_width
    top = (height - target_height) // 2
    bottom = top + target_height
    return frame[top:bottom, left:right]


def to_grayscale(frame):
    if len(frame.shape) == 2:
        return frame
    if frame.shape[2] == 1:
        return frame[:, :, 0]
    return cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)


def split_videos(videos: list[Path], train_ratio: float, val_ratio: float) -> dict[str, list[Path]]:
    total = len(videos)
    train_count = int(total * train_ratio)
    val_count = int(total * val_ratio)

    if train_count + val_count > total:
        raise RuntimeError("train_ratio and val_ratio are too large for the number of videos")

    return {
        "train": videos[:train_count],
        "val": videos[train_count : train_count + val_count],
        "test": videos[train_count + val_count :],
    }


def process_video(
    video_path: Path,
    output_dir: Path,
    crop: tuple[int, int, int, int],
    image_ext: str,
    frame_step: int,
    split_name: str,
    class_name: str,
) -> int:
    class_output_dir = output_dir / split_name / class_name
    class_output_dir.mkdir(parents=True, exist_ok=True)

    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"failed to open video: {video_path}")

    x1, y1, x2, y2 = crop
    saved_count = 0
    frame_index = 0
    read_any_frame = False

    while True:
        ok, frame = capture.read()
        if not ok:
            break

        read_any_frame = True
        center_cropped = center_crop(frame)
        height, width = center_cropped.shape[:2]
        if x2 > width or y2 > height:
            capture.release()
            raise RuntimeError(
                f"crop {crop} exceeds center-cropped frame size {(width, height)} in {video_path}"
            )

        if frame_index % frame_step == 0:
            cropped = to_grayscale(center_cropped[y1:y2, x1:x2])
            output_name = f"{video_path.stem}_frame_{frame_index:06d}{image_ext}"
            output_path = class_output_dir / output_name

            if not cv2.imwrite(str(output_path), cropped):
                capture.release()
                raise RuntimeError(f"failed to save frame: {output_path}")

            saved_count += 1
        frame_index += 1

    capture.release()
    if not read_any_frame:
        raise RuntimeError(f"empty video: {video_path}")
    return saved_count


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert class-folder videos into train/val/test cropped image dataset"
    )
    parser.add_argument(
        "--dataset_dir",
        type=str,
        default="/mnt/hdd16t0/dataset/rps_dataset/datasets",
        help="Input dataset root directory containing class subdirectories",
    )
    parser.add_argument(
        "--output_dir",
        type=str,
        default="/mnt/hdd16t0/dataset/rps_dataset/processed_dataset",
        help="Output dataset root directory for cropped frames",
    )
    parser.add_argument(
        "--crop",
        nargs=4,
        type=int,
        default=DEFAULT_CROP,
        metavar=("X1", "Y1", "X2", "Y2"),
        help="Crop region in pixel coordinates on the 320x320 center-cropped frame: x1 y1 x2 y2",
    )
    parser.add_argument(
        "--frame_step",
        type=int,
        default=3,
        help="Save one frame every N frames. Use 1 to save every frame.",
    )
    parser.add_argument(
        "--train_ratio",
        type=float,
        default=0.8,
        help="Train split ratio by video count",
    )
    parser.add_argument(
        "--val_ratio",
        type=float,
        default=0.1,
        help="Validation split ratio by video count",
    )
    parser.add_argument(
        "--image_ext",
        type=str,
        default=".png",
        choices=[".png", ".jpg", ".jpeg", ".bmp"],
        help="Image extension for saved frames",
    )
    args = parser.parse_args()

    dataset_dir = Path(args.dataset_dir)
    output_dir = Path(args.output_dir)
    crop = parse_crop(list(args.crop))

    if args.frame_step <= 0:
        raise RuntimeError(f"frame_step must be >= 1, got: {args.frame_step}")
    if args.train_ratio < 0 or args.val_ratio < 0:
        raise RuntimeError("train_ratio and val_ratio must be >= 0")
    if args.train_ratio + args.val_ratio >= 1:
        raise RuntimeError("train_ratio + val_ratio must be less than 1")

    if not dataset_dir.exists():
        raise RuntimeError(f"dataset directory not found: {dataset_dir}")

    total_videos = 0
    total_frames = 0
    print(f"Dataset dir: {dataset_dir}")
    print(f"Output dir : {output_dir}")
    print(f"Crop region: {crop}")
    print(f"Frame step : {args.frame_step}")
    print(f"Split ratio : train={args.train_ratio}, val={args.val_ratio}, test={1 - args.train_ratio - args.val_ratio}")

    for class_name in CLASS_NAMES:
        videos = iter_video_files(dataset_dir, class_name)
        splits = split_videos(videos, args.train_ratio, args.val_ratio)
        total_videos += len(videos)
        print(f"Class {class_name}: {len(videos)} videos -> train {len(splits['train'])}, val {len(splits['val'])}, test {len(splits['test'])}")

        for split_name in ("train", "val", "test"):
            for index, video_path in enumerate(splits[split_name], start=1):
                saved_count = process_video(
                    video_path=video_path,
                    output_dir=output_dir,
                    crop=crop,
                    image_ext=args.image_ext,
                    frame_step=args.frame_step,
                    split_name=split_name,
                    class_name=class_name,
                )
                total_frames += saved_count
                print(
                    f"[{class_name} {split_name} {index}/{len(splits[split_name])}] {video_path.name}, saved {saved_count} frames"
                )

    print(f"Finished. Total videos: {total_videos}, total saved frames: {total_frames}")


if __name__ == "__main__":
    main()
