#include "ConsoleApplication.hpp"

namespace shop::ui {
std::string ConsoleApplication::help() {
    return R"(Интернет-магазин: команды, строки с пробелами — в двойных кавычках.
Деньги — целые копейки BYN. Проценты — bps (10000 = 100%).
Режимы учебные, не настоящая аутентификация: as operator | as customer ID.
help | quit | save FILE | load FILE replace
clock show | clock advance SECONDS | clock cleanup

Каталог (чтение в любом режиме):
catalog search [QUERY [CATEGORY|- [name=value ...]]]
catalog variants PRODUCT [name=value ...] | catalog compare PRODUCT PRODUCT ...
Оператор:
category add ID NAME PARENT|- | category move ID PARENT|- | category archive ID
category merge SOURCE TARGET
product add ID CATEGORY TITLE DESCRIPTION
product attribute ID NAME text|integer yes|no [ALLOWED ...]
product publish ID | product archive ID | product restore ID
product move CATEGORY PRODUCT ...
variant add PRODUCT SKU LABEL GRAMS LENGTH_MM WIDTH_MM HEIGHT_MM [name=value ...]
variant enable SKU yes|no
price set SKU KOPECKS | price category CATEGORY CHANGE_BPS
promotion add ID NAME percent|fixed VALUE MINIMUM START END AUTOMATIC_yes|no [CATEGORY ...]
coupon add CODE PROMOTION TOTAL_LIMIT PERSONAL_LIMIT START END | coupon disable CODE
coupon usage CODE [CUSTOMER — только оператору; покупатель видит свою квоту]
receipt post ID SUPPLIER SKU QUANTITY PURCHASE_COST BATCH [SKU QUANTITY PURCHASE_COST BATCH ...]
stock show | stock count SKU ACTUAL | stock needs | stock audit
customer add ID NAME EMAIL PHONE | customer restrict ID REASON | customer restore ID

Покупатель:
address list | address add LABEL INDEX CITY STREET BUILDING APARTMENT
address replace LABEL INDEX CITY STREET BUILDING APARTMENT
address default LABEL | address remove LABEL
wish show | wish add PRODUCT | wish remove PRODUCT | wish purge
wish to-cart [PRODUCT SKU ...]
cart show | cart add SKU QUANTITY | cart quantity SKU QUANTITY | cart remove SKU
cart coupon CODE|- | cart adjust | cart prune | cart switch OLD_SKU NEW_SKU
cart clear | cart wish OPERATION_ID SKU ...
order preview DELIVERY ADDRESS_LABEL|- | order place OPERATION_ID
order list [unpaid|paid|picking|shipped|delivered|cancelled|expired|- [START END]]
order show ORDER
order pay ORDER KOPECKS card|transfer yes|no OPERATION_ID [DECLINE_REASON]
order cancel ORDER OPERATION_ID | order extend ORDER OPERATION_ID
order delivery-preview ORDER DELIVERY ADDRESS_LABEL|- | order delivery-confirm OPERATION_ID
order repeat-plan ORDER | order repeat ORDER OPERATION_ID SKU ...
order deliver ORDER OPERATION_ID
return open ORDER OPERATION_ID SKU QUANTITY REASON [SKU QUANTITY REASON ...]
return amend REQUEST OPERATION_ID SKU QUANTITY REASON [SKU QUANTITY REASON ...]
return withdraw REQUEST OPERATION_ID | return show REQUEST | return list
REASON: changed-mind | defective | wrong-item
review add ORDER PRODUCT RATING TEXT | review edit REVIEW RATING TEXT | review vote REVIEW
review rating PRODUCT | review list PRODUCT [helpful|high|low|id [OFFSET LIMIT]]

Исполнение/возвраты/отчёты (оператор):
fulfill start ORDER OPERATION_ID | fulfill show ORDER
fulfill pick ORDER SKU QUANTITY OPERATION_ID | fulfill undo ORDER SKU QUANTITY OPERATION_ID
fulfill pack ORDER OPERATION_ID | fulfill reopen ORDER OPERATION_ID
fulfill ship ORDER TRACKING_NUMBER OPERATION_ID
return inspect REQUEST OPERATION_ID SKU RECEIVED ACCEPTED RESTOCK CONDITION [SKU ...]
CONDITION: unopened | opened | defective | incomplete
return reject REQUEST OPERATION_ID REASON | return complete REQUEST OPERATION_ID
review moderate REVIEW yes|no [HIDE_REASON]
report START END

Начальные данные: alice/bob, адрес alice: home; SHIRT-BLUE/SHIRT-RED;
доставка standard/express/pickup, купон WELCOME. --demo выполняет полный пример.
Сохранение явное; load заменяет магазин и сбрасывает предпросмотры.
)";
}
} // namespace shop::ui
