<?php
function cause_segfault($depth) {
	return cause_segfault($depth + 1);
}
?>