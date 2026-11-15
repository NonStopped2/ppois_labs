#!/usr/bin/env python3
"""Black-box CLI checks; domain unit tests remain separate C++ Catch2 tests."""
import argparse
import subprocess
import sys
import unittest
from pathlib import Path

EXECUTABLE = None
EXAMPLES_DIR = Path(__file__).resolve().parents[1] / "examples"


def command(section, action, *fields):
    return [str(section), str(action), *map(str, fields)]


def quote(number="BY-1", invoice="i1"):
    return command(3, 1, number, invoice, "standard")


def pay(invoice="i1", amount=300, payment="p1", method=1):
    return command(3, 3, "A", invoice, payment, amount, method)


def accept(number="BY-1", invoice="i1"):
    return command(4, 1, "A", number, invoice)


def advance(seconds):
    return command(9, 2, seconds)


def document(holder):
    return [1, "AB123", holder, "2020-01-01", "2030-12-31"]


class CliTests(unittest.TestCase):
    def run_cli(self, fields=(), args=("--demo",), expected=0):
        completed = subprocess.run([str(EXECUTABLE), *args], input="\n".join(map(str, fields)) + "\n",
                                   text=True, capture_output=True, timeout=15)
        self.assertEqual(completed.returncode, expected, completed.stdout + completed.stderr)
        self.assertNotIn("AddressSanitizer", completed.stderr)
        self.assertNotIn("runtime error:", completed.stderr)
        return completed.stdout + completed.stderr

    def test_documented_replay_examples(self):
        examples = EXAMPLES_DIR
        for name, expected in (("transport", "Letter | Delivered"),
                               ("courier", "Parcel | Delivered"), ("loss", "Letter | Lost")):
            fields = (examples / f"{name}.input").read_text(encoding="utf-8").splitlines()
            output = self.run_cli(fields)
            self.assertIn(expected, output)
            self.assertNotIn("Ошибка операции", output)
            self.assertNotIn("Ошибка ввода:", output)

    def test_help_arguments_and_eof(self):
        self.assertIn("post_cli", self.run_cli(args=("--help",)))
        self.assertIn("Неизвестные аргументы", self.run_cli(args=("--unknown",), expected=2))
        self.assertIn("Ввод завершён", self.run_cli(fields=(), args=()))

    def test_customer_creation_and_draft_cancellation(self):
        fields = command(1, 1, "custom", "Иван Клиент", "220001", "Минск", "Ленина", "2", "", "+375291112233", "")
        fields += command(1, 2) + command(2, 5, "BY-1", "Передумал") + command(2, 4, "BY-1") + [0]
        output = self.run_cli(fields)
        self.assertIn("custom | Иван Клиент", output)
        self.assertIn("Letter | Cancelled", output)
        self.assertNotIn("Ошибка операции", output)

    def test_payment_refund_cash_card_and_failed_close(self):
        fields = quote() + pay(amount=100) + pay(amount=200, payment="p2", method=2)
        fields += command(3, 4, "A", "i1", "p1", "r1", 50, "Частичный возврат")
        fields += command(8, 4, "A") + command(8, 5, "A", 0, 0) + command(8, 4, "A") + [0]
        output = self.run_cli(fields)
        self.assertIn("долг: 0.50 BYN", output)
        self.assertIn("наличность 500.50 BYN", output)
        self.assertIn("Ошибка операции", output)
        self.assertEqual(output.count("открыта=1"), 2)

    def test_transport_and_authorized_pickup(self):
        fields = quote() + pay() + accept()
        fields += command(5, 1, "t1", "A", "B", "van-1", "BY-1") + advance(300)
        fields += command(5, 2, "t1") + advance(1200) + command(5, 3, "t1", "B")
        fields += advance(300) + command(5, 4, "t1", "B")
        fields += command(4, 3, "B", "BY-1", "recipient", *document("Борис Получатель"), "")
        fields += command(5, 6) + command(2, 4, "BY-1") + [0]
        output = self.run_cli(fields)
        self.assertIn("Letter | Delivered", output)
        self.assertIn("назначение свободна", output)
        self.assertNotIn("Ошибка операции", output)

    def test_courier_failure_return_retry_and_representative(self):
        fields = quote("BY-2", "i2") + pay("i2", 550) + accept("BY-2", "i2")
        fields += command(6, 1, "d1", "A", "BY-2", 0, 3600) + command(6, 2, "d1")
        fields += command(6, 3, "d1", "d1-BY-2", "f1", 0, 1, "Нет дома") + command(6, 4, "d1")
        fields += advance(3600) + command(6, 5, "d1", "d1-BY-2", "d2", 0, 3600)
        fields += command(4, 4, "auth", "BY-2", "rep", 3600) + command(6, 2, "d2")
        fields += command(6, 3, "d2", "d1-BY-2", "success", 1, "rep", *document("Глеб Представитель"), "auth")
        fields += command(6, 4, "d2") + command(6, 6) + command(2, 4, "BY-2") + [0]
        output = self.run_cli(fields)
        self.assertIn("Parcel | Delivered", output)
        self.assertIn("попыток=2", output)
        self.assertNotIn("Ошибка операции", output)

    def test_expired_unstarted_courier_route_can_be_cancelled_and_replanned(self):
        fields = quote("BY-2", "i2") + pay("i2", 550) + accept("BY-2", "i2")
        fields += command(6, 1, "d1", "A", "BY-2", 0, 60) + advance(61)
        fields += command(6, 2, "d1") + command(6, 7, "d1") + command(4, 2, "A")
        fields += command(6, 5, "d1", "d1-BY-2", "d2", 0, 60) + command(6, 2, "d2")
        fields += command(6, 3, "d2", "d1-BY-2", "done", 1, "local", *document("Вера Местная"), "")
        fields += command(6, 4, "d2") + command(6, 6) + command(2, 4, "BY-2") + [0]
        output = self.run_cli(fields)
        self.assertEqual(output.count("Ошибка операции"), 1)
        self.assertIn("Курьерский план отменён", output)
        self.assertIn("BY-2 | A-general", output)
        self.assertIn("state=Cancelled", output)
        self.assertIn("Parcel | Delivered", output)
        self.assertIn("попыток=1", output)

    def test_expired_active_courier_route_returns_without_fictitious_attempt(self):
        fields = quote("BY-2", "i2") + pay("i2", 550) + accept("BY-2", "i2")
        fields += command(6, 1, "d1", "A", "BY-2", 0, 60) + command(6, 2, "d1") + advance(61)
        fields += command(6, 3, "d1", "d1-BY-2", "late", 0, 1, "Не застали") + command(6, 4, "d1")
        fields += command(6, 8, "d1") + command(6, 6) + command(4, 2, "A")
        fields += command(6, 5, "d1", "d1-BY-2", "d2", 0, 60) + command(6, 2, "d2")
        fields += command(6, 3, "d2", "d1-BY-2", "done", 1, "local", *document("Вера Местная"), "")
        fields += command(6, 4, "d2") + command(6, 6) + command(2, 4, "BY-2") + [0]
        output = self.run_cli(fields)
        self.assertEqual(output.count("Ошибка операции"), 2)
        self.assertIn("Курьерский маршрут прерван", output)
        self.assertIn("state=ReturnedToBranch | попыток=0", output)
        self.assertIn("BY-2 | A-general", output)
        self.assertIn("Parcel | Delivered", output)
        self.assertIn("попыток=1", output)
        self.assertNotIn("DeliveryFailed", output)

    def test_return_and_original_sender_handover(self):
        def transport(trip, source, target):
            return (command(5, 1, trip, source, target, "van-1", "BY-1") + advance(300)
                    + command(5, 2, trip) + advance(1200) + command(5, 3, trip, target)
                    + advance(300) + command(5, 4, trip, target))
        fields = quote() + pay() + accept() + transport("out", "A", "B")
        fields += command(7, 1, "r", "BY-1", 1, "sender") + command(7, 2, "r", "B", 1)
        fields += command(7, 3, "r", "B") + transport("back", "B", "A")
        fields += command(4, 3, "A", "BY-1", "sender", *document("Анна Отправитель"), "")
        fields += command(2, 4, "BY-1") + [0]
        output = self.run_cli(fields)
        self.assertIn("Letter | Returned", output)
        self.assertNotIn("Ошибка операции", output)

    def test_eof_in_parcel_does_not_register_half_ready_prototype(self):
        fields = command(2, 2, "sender", "local", 500, 200, 100, 50, 0, 0, 1, 0, 0, 1, 2,
                         "Книга", 1, 100, 1000, 2)
        output = self.run_cli(fields)
        self.assertIn("Незавершённая команда отменена", output)
        self.assertNotIn("Создано отправление BY-3", output)

    def test_late_trip_error_does_not_reserve_id_or_remove_warehouse_cargo(self):
        fields = quote() + pay() + accept()
        fields += command(5, 1, "t", "A", "B", "van-1", "BY-1,BY-2")
        fields += command(5, 1, "t", "A", "B", "van-1", "BY-1") + command(4, 2, "A") + [0]
        output = self.run_cli(fields)
        self.assertIn("Ошибка операции", output)
        self.assertIn("План рейса t готов", output)
        self.assertIn("BY-1 | A-general", output)

    def test_expired_shift_cannot_be_replaced_early_but_can_be_renewed_at_end(self):
        fields = command(9, 3, "A") + advance(86400) + command(5, 5, "A", "van-1")
        fields += command(9, 3, "A") + command(5, 5, "A", "van-1", 1) + [0]
        output = self.run_cli(fields)
        self.assertEqual(output.count("Ошибка операции"), 2)
        self.assertIn("Новые смены открыты", output)
        self.assertIn("Допуск обновлён", output)

    def test_storage_expiry_return_checks_current_deadline(self):
        fields = quote() + pay() + accept() + advance(7 * 86400) + command(9, 3, "A")
        fields += command(7, 1, "expired", "BY-1", 3, "A") + command(7, 2, "expired", "A", 1)
        fields += command(7, 3, "expired", "A") + command(7, 7) + [0]
        output = self.run_cli(fields)
        self.assertNotIn("Ошибка операции", output)
        self.assertIn("state=Started", output)

    def test_search_cannot_close_before_all_checks_and_waiting_period(self):
        fields = quote() + pay() + accept() + command(7, 4, "s", "BY-1", "sender", "A,B")
        fields += command(7, 5, "s", "A", 0, "Нет") + command(7, 6, "s", "A", 0)
        fields += advance(3600) + command(7, 6, "s", "A", 0) + command(2, 4, "BY-1") + [0]
        output = self.run_cli(fields)
        self.assertEqual(output.count("Ошибка операции"), 2)
        self.assertIn("Letter | Accepted", output)
        self.assertNotIn("Letter | Lost", output)

    def test_loss_search_compensation_and_repeated_payout(self):
        fields = quote() + pay() + accept()
        fields += command(7, 4, "s1", "BY-1", "sender", "A,B")
        fields += command(7, 5, "s1", "A", 0, "Не обнаружено") + command(7, 5, "s1", "B", 0, "Не поступало")
        fields += advance(3600) + command(7, 6, "s1", "A", 0)
        fields += command(8, 1, "c1", "BY-1", "sender", 500, 1, "report-1")
        fields += command(8, 2, "c1", "A", 1, 500, 500, "s1") + command(8, 3, "A", "c1")
        fields += command(8, 3, "A", "c1") + command(8, 4, "A") + command(2, 4, "BY-1") + [0]
        output = self.run_cli(fields)
        self.assertIn("Letter | Lost", output)
        self.assertEqual(output.count("Компенсация выплачена."), 1)
        self.assertIn("наличность 498.00 BYN", output)
        self.assertIn("Ошибка операции", output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--examples-dir", type=Path, default=EXAMPLES_DIR)
    arguments, test_arguments = parser.parse_known_args()
    EXECUTABLE = arguments.executable.resolve()
    EXAMPLES_DIR = arguments.examples_dir.resolve()
    if not EXECUTABLE.is_file():
        parser.error(f"Missing executable: {EXECUTABLE}")
    unittest.main(argv=[sys.argv[0], *test_arguments])
