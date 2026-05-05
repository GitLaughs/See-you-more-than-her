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


def count_selected_frames(frame_count: int, frame_step: int) -> int:
    return (frame_count + frame_step - 1) // frame_step


def split_by_frames(total_frames: int, train_ratio: float, val_ratio: float) -> tuple[int, int, int]:
    train_count = int(total_frames * train_ratio)
    val_count = int(total_frames * val_ratio)
    if train_count + val_count > total_frames:
        raise RuntimeError("train_ratio and val_ratio are too large for selected frames")
    return train_count, val_count, total_frames - train_count - val_count


def split_name_for_index(index: int, train_end: int, val_end: int) -> str:
    if index < train_end:
        return "train"
    if index < val_end:
        return "val"
    return "test"


def process_video(
    video_path: Path,
    output_dir: Path,
    crop: tuple[int, int, int, int],
    image_ext: str,
    frame_step: int,
    class_name: str,
    selected_offset: int,
    train_end: int,
    val_end: int,
) -> tuple[dict[str, int], int]:
    capture = cv2.VideoCapture(str(video_path))
    if not capture.isOpened():
        raise RuntimeError(f"failed to open video: {video_path}")

    x1, y1, x2, y2 = crop
    split_counts = {"train": 0, "val": 0, "test": 0}
    saved_count = 0
    selected_index = selected_offset
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
            split_name = split_name_for_index(selected_index, train_end, val_end)
            class_output_dir = output_dir / split_name / class_name
            class_output_dir.mkdir(parents=True, exist_ok=True)

            cropped = to_grayscale(center_cropped[y1:y2, x1:x2])
            output_name = f"{video_path.stem}_frame_{frame_index:06d}{image_ext}"
            output_path = class_output_dir / output_name

            if not cv2.imwrite(str(output_path), cropped):
                capture.release()
                raise RuntimeError(f"failed to save frame: {output_path}")

            split_counts[split_name] += 1
            saved_count += 1
            selected_index += 1
        frame_index += 1

    capture.release()
    if not read_any_frame:
        raise RuntimeError(f"empty video: {video_path}")
    return split_counts, saved_count


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert class-folder videos into train/val/test cropped image dataset")
    parser.add_argument("--dataset_dir", type=str, default="data/rps_dataset/datasets")
    parser.add_argument("--output_dir", type=str, default="data/rps_dataset/processed_dataset")
    parser.add_argument("--crop", nargs=4, type=int, default=DEFAULT_CROP, metavar=("X1", "Y1", "X2", "Y2"))
    parser.add_argument("--frame_step", type=int, default=3)
    parser.add_argument("--train_ratio", type=float, default=0.8)
    parser.add_argument("--val_ratio", type=float, default=0.1)
    parser.add_argument("--image_ext", type=str, default=".png", choices=[".png", ".jpg", ".jpeg", ".bmp"])
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
        selected_counts = []
        for video_path in videos:
            capture = cv2.VideoCapture(str(video_path))
            if not capture.isOpened():
                raise RuntimeError(f"failed to open video: {video_path}")
            frame_count = int(capture.get(cv2.CAP_PROP_FRAME_COUNT))
            capture.release()
            if frame_count <= 0:
                raise RuntimeError(f"failed to read frame count: {video_path}")
            selected_counts.append(count_selected_frames(frame_count, args.frame_step))

        total_selected_frames = sum(selected_counts)
        train_end, val_count, test_count = split_by_frames(total_selected_frames, args.train_ratio, args.val_ratio)
        val_end = train_end + val_count
        total_videos += len(videos)
        print(f"Class {class_name}: {len(videos)} videos, {total_selected_frames} selected frames -> train {train_end}, val {val_count}, test {test_count}")

        class_split_counts = {"train": 0, "val": 0, "test": 0}
        class_selected_offset = 0
        for index, (video_path, selected_count) in enumerate(zip(videos, selected_counts), start=1):
            split_counts, saved_count = process_video(
                video_path=video_path,
                output_dir=output_dir,
                crop=crop,
                image_ext=args.image_ext,
                frame_step=args.frame_step,
                class_name=class_name,
                selected_offset=class_selected_offset,
                train_end=train_end,
                val_end=val_end,
            )
            class_selected_offset += selected_count
            total_frames += saved_count
            for split_name in class_split_counts:
                class_split_counts[split_name] += split_counts[split_name]
            print(f"[{class_name} {index}/{len(videos)}] {video_path.name}, saved {saved_count} frames")

        print(f"Class {class_name} split frames: train={class_split_counts['train']} val={class_split_counts['val']} test={class_split_counts['test']}")

    print(f"Finished. Total videos: {total_videos}, total saved frames: {total_frames}")


if __name__ == "__main__":
    main()
