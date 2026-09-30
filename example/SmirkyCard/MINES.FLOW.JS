scenario('first', function (c) {
  function expect(id, text) {
    if (c.test.text(id) !== text) throw Error(id + ': expected ' + text);
  }
  c.run(Flow()
    .step(function () {
      expect('Mines.Status', 'Mines left: 10');
    }).named('initial-status')
    .step(function () {
      c.test.click('Mines.Flag');
      expect('Mines.Flag', 'Flag mode: on');
      c.test.click('Mines.Cell.63');
      expect('Mines.Cell.63', 'F');
      expect('Mines.Status', 'Mines left: 9');
    }).named('flag-cell')
    .step(function () {
      c.test.click('Mines.Flag');
      expect('Mines.Flag', 'Flag mode: off');
    }).named('reveal-mode')
    .step(function () {
      c.test.click('Mines.Cell.0');
      expect('Mines.Cell.0', '');
      expect('Mines.Cell.1', '');
      expect('Mines.Cell.8', '');
      expect('Mines.Cell.9', '');
      expect('Mines.Cell.2', '1');
      expect('Mines.Cell.10', '1');
      expect('Mines.Cell.11', '2');
      expect('Mines.Cell.63', 'F');
      expect('Mines.Status', 'Mines left: 9');
    }).named('flood-fill')
    .step(function () {
      c.test.log('One flag placed; zero-cell flood opened neighbours.');
    }).named('summary'));
});
