#!/usr/bin/env python3
"""Black-box checks of the actual store executable, including cross-process snapshots."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

BINARY = None
EXAMPLES = Path(__file__).resolve().parents[1] / "examples"
DELIVERED_PURCHASE = (
    "as customer alice\ncart add SHIRT-BLUE 3\norder preview standard home\n"
    "order place place\norder pay order-1 3738 card yes pay\nas operator\n"
    "fulfill start order-1 start\nfulfill pick order-1 SHIRT-BLUE 3 pick\n"
    "fulfill pack order-1 pack\nfulfill ship order-1 TRACK ship\nas customer alice\n"
    "order deliver order-1 deliver\n")


class StoreCliTests(unittest.TestCase):
    def run_cli(self, script="", arguments=None, expected=0):
        result = subprocess.run([str(BINARY), *(arguments if arguments is not None else ["--script", "-"])],
                                input=script, text=True, encoding="utf-8", capture_output=True, timeout=20)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_help_demo_and_clean_eof(self):
        self.assertIn("save FILE", self.run_cli(arguments=["--help"]))
        self.assertIn("Сверка: 0 расхождений", self.run_cli(arguments=["--demo"]))
        self.assertIn("Интернет-магазин", self.run_cli(arguments=[]))
        self.assertEqual(self.run_cli(), "")

    def test_argument_and_input_errors_terminate(self):
        for arguments in (["--unknown"], ["--script"], ["--load"], ["--demo", "--script", "-"],
                          ["--script", "-", "--script", "-"]):
            self.assertIn("Ошибка запуска", self.run_cli(arguments=arguments, expected=1))
        for line in ('"unfinished', 'as customer alice\ncart add SHIRT-BLUE 1x',
                     'as customer alice\ncart add SHIRT-BLUE -1', 'as nobody', 'unknown'):
            self.assertIn("Ошибка", self.run_cli(line + "\n", expected=1))

    def test_installed_example(self):
        examples = {"purchase.input": "Сверка: 0 расхождений",
                    "coupon_cancel.input": "Расхождений: 0", "new_product.input": "ИТОГО:"}
        for name, marker in examples.items():
            with self.subTest(example=name):
                output = self.run_cli(arguments=["--script", str(EXAMPLES / name)])
                self.assertIn(marker, output)

    def test_real_files_across_processes_keep_idempotent_payment(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-files-") as directory:
            snapshot = Path(directory) / "store with spaces.json"
            path = json.dumps(str(snapshot), ensure_ascii=False)
            script = ("as customer alice\ncart add SHIRT-BLUE 3\norder preview standard home\n"
                      "order place place\norder pay order-1 3738 card yes pay\nas operator\n"
                      f"save {path}\n")
            self.run_cli(script)
            before = json.loads(snapshot.read_text(encoding="utf-8"))
            self.run_cli("as customer alice\norder pay order-1 3738 card yes pay\nas operator\n"
                         f"save {path}\n", ["--load", str(snapshot), "--script", "-"])
            self.assertEqual(json.loads(snapshot.read_text(encoding="utf-8")), before)
            output = self.run_cli(f"load {path} replace\norder show order-1\n")
            self.assertIn("Confirmed", output)

    def test_rejected_commands_preserve_cart_and_orders(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-invalid-") as directory:
            snapshot = Path(directory) / "state.json"
            path = json.dumps(str(snapshot))
            self.run_cli("as customer alice\ncart add SHIRT-BLUE 3 extra\n"
                         "as operator\nclock advance -1\n" + f"save {path}\n", expected=1)
            state = json.loads(snapshot.read_text(encoding="utf-8"))
            self.assertEqual(state["clock"], 1000)
            self.assertEqual(state["orders"]["entries"], [])
            self.assertEqual(state["customers"][0]["cart"]["lines"], [])

    def test_price_change_requires_new_confirmation(self):
        output = self.run_cli("as customer alice\ncart add SHIRT-BLUE 1\norder preview standard home\n"
                              "as operator\nprice set SHIRT-BLUE 1200\nas customer alice\norder place stale\n",
                              expected=1)
        self.assertIn("Условия покупки изменились", output)
        self.assertNotIn("Заказ: order-", output)

    def test_zero_total_delivery_change_is_rejected(self):
        output = self.run_cli("promotion add full Полная percent 10000 0 0 99999 no\n"
                              "coupon add FULL full 10 1 0 99999\nas customer alice\n"
                              "cart add SHIRT-BLUE 1\ncart coupon FULL\norder preview standard home\n"
                              "order place place\norder delivery-preview order-1 pickup home\norder show order-1\n",
                              expected=1)
        self.assertIn("нулевой заказ", output)
        self.assertIn("AwaitingPayment | 5.00 BYN", output)

    def test_corrupt_load_keeps_old_store_and_quote(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-load-") as directory:
            broken = Path(directory) / "broken.json"
            broken.write_text('{"format":', encoding="utf-8")
            output = self.run_cli("as customer alice\ncart add SHIRT-BLUE 1\n"
                                  "as operator\n" + f"load {json.dumps(str(broken))} replace\n"
                                  "as customer alice\ncart show\n", expected=1)
            self.assertIn("SHIRT-BLUE x 1", output)

    def test_restored_expiry_skips_caller_keys_and_repeated_cleanup_is_harmless(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-expiry-") as directory:
            file = Path(directory) / "shop.json"
            path = json.dumps(str(file))
            base = "system-expire-order-1"
            self.run_cli("as customer alice\ncart add SHIRT-BLUE 3\norder preview standard home\n"
                         f"order place {base}\norder extend order-1 {base}-1\nas operator\nsave {path}\n")
            before = json.loads(file.read_text(encoding="utf-8"))
            output = self.run_cli(f"clock advance 4000\nclock cleanup\norder show order-1\nsave {path}\n",
                                  ["--load", str(file), "--script", "-"])
            self.assertIn("Expired", output)
            after = json.loads(file.read_text(encoding="utf-8"))
            self.assertTrue(all(item["reserved"] == 0 for item in after["inventory"]["items"]))
            old = {entry["id"]: entry for entry in before["operations"]}
            new = {entry["id"]: entry for entry in after["operations"]}
            for key, record in old.items():
                self.assertEqual(new[key], record)
            self.assertIn(base + "-2", new)
            self.run_cli(f"clock cleanup\nsave {path}\n", ["--load", str(file), "--script", "-"])
            self.assertEqual(json.loads(file.read_text(encoding="utf-8")), after)

    def test_restored_return_expiry_cannot_be_blocked_by_caller_operation_keys(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-return-expiry-") as directory:
            file = Path(directory) / "shop.json"
            path = json.dumps(str(file))
            base = "system-expire-return-return-6"
            self.run_cli(DELIVERED_PURCHASE +
                         f"return open order-1 {base} SHIRT-BLUE 1 changed-mind\n"
                         f"return amend return-6 {base}-1 SHIRT-BLUE 1 changed-mind\n"
                         f"as operator\nsave {path}\n")
            before = json.loads(file.read_text(encoding="utf-8"))
            request = before["returns"]["requests"][0]
            advance = request["deadline"] - before["clock"]
            self.run_cli(f"clock advance {advance}\nclock cleanup\nsave {path}\n",
                         ["--load", str(file), "--script", "-"])
            after = json.loads(file.read_text(encoding="utf-8"))
            self.assertEqual(after["returns"]["requests"][0]["decision_reason"], "NotReceivedInTime")
            self.assertEqual(after["inventory"], before["inventory"])
            self.assertEqual(after["payments"], before["payments"])
            self.assertIn(base + "-2", {entry["id"] for entry in after["operations"]})
            self.run_cli(f"clock cleanup\nsave {path}\n", ["--load", str(file), "--script", "-"])
            self.assertEqual(json.loads(file.read_text(encoding="utf-8")), after)

    def test_failed_payment_and_partial_inspection_preserve_complete_saved_state(self):
        with tempfile.TemporaryDirectory(prefix="shop-cli-rollback-") as directory:
            file = Path(directory) / "shop.json"
            path = json.dumps(str(file))
            self.run_cli("as customer alice\ncart add SHIRT-BLUE 3\norder preview standard home\n"
                         f"order place place\nas operator\nsave {path}\n")
            before = json.loads(file.read_text(encoding="utf-8"))
            self.run_cli("as customer alice\norder pay order-1 1 card yes pay\n"
                         f"as operator\nsave {path}\n",
                         ["--load", str(file), "--script", "-"], expected=1)
            self.assertEqual(json.loads(file.read_text(encoding="utf-8")), before)
            self.run_cli(DELIVERED_PURCHASE +
                         f"return open order-1 open SHIRT-BLUE 2 changed-mind\nas operator\nsave {path}\n")
            before = json.loads(file.read_text(encoding="utf-8"))
            self.run_cli(f"return inspect return-6 inspect SHIRT-BLUE 1 1 1 opened\nsave {path}\n",
                         ["--load", str(file), "--script", "-"], expected=1)
            self.assertEqual(json.loads(file.read_text(encoding="utf-8")), before)
            output = self.run_cli("return inspect return-6 inspect SHIRT-BLUE 2 2 2 opened\n",
                                  ["--load", str(file), "--script", "-"])
            self.assertIn("inspection-7", output)


def main():
    global BINARY, EXAMPLES
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--examples-dir", type=Path, default=EXAMPLES)
    args, remaining = parser.parse_known_args()
    BINARY = args.binary.resolve()
    EXAMPLES = args.examples_dir.resolve()
    if not BINARY.is_file():
        parser.error(f"Missing store executable: {BINARY}")
    unittest.main(argv=[__file__, *remaining])


if __name__ == "__main__":
    main()
