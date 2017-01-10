<?php
require_once('visual/site.php');
require_once('renderer/WikiRenderer.lib.php');
require_once(WIKIRENDERER_PATH . 'rules/dokuwiki_to_xhtml.php');

function BeginsWith($strHaystack, $strNeedle)
{
	return substr($strHaystack,0,strlen($strNeedle)) == $strNeedle;
}

function GetTagValue($strHaystack, $tag)
{
	if (!BeginsWith($strHaystack, $tag.':'))
		return null;
	return substr($strHaystack,strlen($tag)+1);
}


class HR extends WikiRendererBloc {
    public $type='hr';
    protected $regexp="/^----$/";
    protected $_closeNow=true;

    public function getRenderedLine(){
        return '<hr />';
    }
}

class MyConfig extends dokuwiki_to_xhtml
{
	public function MyConfig()
	{
		$this->bloctags[] = 'HR';
	}
	
	
	
	/**
	* in some wiki system, some links are specials. You should override this method
	* to transform this specific links to real URL
	* @return array  first item is the url, second item is an alternate label
	*/
	public function processLink($url, $tagName='')
	{
		$rs = parent::processLink($url, $tagName);
		$url = $rs[0];
		if ($appendix = GetTagValue($url,'img'))
			return array("./images/$appendix", $rs[1]);
		$name = $rs[1];
		$at = strrpos($name,'/');
		if ($at !== FALSE)
			$name = substr($name,$at+1);
		return array("./html.php?page=".strtolower($url), $name);
	}
};

$ctr=new WikiRenderer(new MyConfig());

function Convert($strPage)
{
	global $ctr;
	if (!$strPage)
		$strPage = "home";
	$strPage = str_replace(array('.'),'',$strPage);
	$text = file_get_contents("./documentation/$strPage.prefix").file_get_contents("./documentation/$strPage.txt");
	$text = preg_replace('/{{(\s?)([^:]+)}}/','{{\1img:\2}}',$text);

	$text = $ctr->render($text);
	$rs = array();
	$rs[] = "Main pages: <a class='documentation_link' href='html.php?page=home'>Welcome</a>"
			."<a class='documentation_link' href='html.php?page=controls'>Controls</a>"
			."<a class='documentation_link' href='html.php?page=units/list'>Units</a>"
			;
	if($ctr->errors)
	{
		//echo '<p style="color:red;">Errors in line(s):';
		foreach ($ctr->errors as $line => $l)
			$rs[] = "$line: $l";
	}
	$text = preg_replace('/%([a-zA-Z]+)%/','<a class="image" href="html.php?page=concepts/resources"><img src=\'images/\1.png\' title=\'\1\' alt=\'\1\' /></a>',$text);
	$rs[] = $text;
	return $rs;
}
//print_r($_GET);
//echo $_GET['page'];
Site(Convert($_GET['page']),"Documentation");